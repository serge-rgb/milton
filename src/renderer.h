//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


// Returns false if there were allocation errors
func b32 render_tile(MiltonState* milton_state,
                     Arena* tile_arena,
                     Rect* blocks,
                     i32 block_start, i32 num_blocks,
                     u32* raster_buffer)
{
    Rect raster_tile_rect = { 0 };
    Rect canvas_tile_rect = { 0 };

    const i32 blocks_per_tile = milton_state->blocks_per_tile;
    // Clip and move to canvas space.
    // Derive tile_rect
    for (i32 block_i = 0; block_i < blocks_per_tile; ++block_i)
    {
        if (block_start + block_i >= num_blocks)
        {
            break;
        }
        blocks[block_start + block_i] = rect_clip_to_screen(blocks[block_start + block_i],
                                                  milton_state->view->screen_size);
        raster_tile_rect = rect_union(raster_tile_rect, blocks[block_start + block_i]);

        canvas_tile_rect.top_left =
                raster_to_canvas(milton_state->view, raster_tile_rect.top_left);
        canvas_tile_rect.bot_right =
                raster_to_canvas(milton_state->view, raster_tile_rect.bot_right);
    }

    // Filter strokes to this tile.
    b32* stroke_masks = filter_strokes_to_rect(tile_arena,
                                               milton_state->strokes,
                                               milton_state->num_strokes,
                                               canvas_tile_rect);
    if (!stroke_masks)
    {
        return false;
    }

    b32 allocation_ok = true;
    for (int block_i = 0; block_i < blocks_per_tile; ++block_i)
    {
        if (block_start + block_i >= num_blocks)
        {
            break;
        }

        allocation_ok = rasterize_canvas_block(tile_arena,
                                               milton_state->view,
                                               milton_state->strokes,
                                               stroke_masks,
                                               milton_state->num_strokes,
                                               &milton_state->working_stroke,
                                               raster_buffer,
                                               blocks[block_start + block_i]);
        if (!allocation_ok)
        {
            return false;
        }
    }
    return true;
}

typedef struct
{
    MiltonState* milton_state;
    i32 worker_id;
} WorkerParams;

func int render_worker(void* data)
{
    WorkerParams* params = (WorkerParams*) data;
    MiltonState* milton_state = params->milton_state;
    i32 id = params->worker_id;
    RenderQueue* render_queue = milton_state->render_queue;

    for (;;)
    {
        int err = SDL_SemWait(render_queue->work_available);
        if (err != 0)
        {
            milton_fatal("Failure obtaining semaphore inside render worker");
        }
        err = SDL_LockMutex(render_queue->mutex);
        if (err != 0)
        {
            milton_fatal("Failure locking render queue mutex");
        }
        i32 index = -1;
        TileRenderData tile_data = { 0 };
        if (!err)
        {
            index = --render_queue->index;
            assert (index >= 0);
            assert (index <  RENDER_QUEUE_SIZE);
            tile_data = render_queue->tile_render_data[index];
            SDL_UnlockMutex(render_queue->mutex);
        }
        else { assert (!"Render worker not handling lock error"); }

        assert (index >= 0);

        b32 allocation_ok =
                render_tile(milton_state,
                            &milton_state->render_worker_arenas[id],
                            render_queue->blocks,
                            tile_data.block_start, render_queue->num_blocks,
                            render_queue->raster_buffer);
        if (!allocation_ok)
        {
            milton_state->worker_needs_memory = true;
        }

        arena_reset(&milton_state->render_worker_arenas[id]);

        SDL_SemPost(render_queue->completed_semaphore);
    }
}

func void produce_render_work(MiltonState* milton_state, TileRenderData tile_render_data)
{
    RenderQueue* render_queue = milton_state->render_queue;
    i32 lock_err = SDL_LockMutex(milton_state->render_queue->mutex);
    if (!lock_err)
    {
        render_queue->tile_render_data[render_queue->index++] = tile_render_data;
        SDL_UnlockMutex(render_queue->mutex);
    }
    else { assert (!"Lock failure not handled"); }

    SDL_SemPost(render_queue->work_available);
}


// Returns true if operation was completed.
func b32 render_canvas(MiltonState* milton_state, u32* raster_buffer, Rect raster_limits)
{
    Rect* blocks = NULL;

    i32 num_blocks = rect_split(milton_state->transient_arena,
                                raster_limits,
                                milton_state->block_width, milton_state->block_width, &blocks);

    if (num_blocks < 0)
    {
        milton_log ("[ERROR] Transient arena did not have enough memory for canvas block.\n");
        milton_fatal("Not handling this error.");
    }

    RenderQueue* render_queue = milton_state->render_queue;
    {
        render_queue->blocks = blocks;
        render_queue->num_blocks = num_blocks;
        render_queue->raster_buffer = raster_buffer;
    }

    const i32 blocks_per_tile = milton_state->blocks_per_tile;

    size_t render_memory_cap = milton_state->worker_memory_size;

    for (int i = 0; i < milton_state->num_render_workers; ++i)
    {
        assert(milton_state->render_worker_arenas[i].ptr == NULL);
        milton_state->render_worker_arenas[i] = arena_init(calloc(render_memory_cap, 1),
                                                           render_memory_cap);
        assert(milton_state->render_worker_arenas[i].ptr != NULL);
    }

    i32 tile_acc = 0;
    for (int block_i = 0; block_i < num_blocks; block_i += blocks_per_tile)
    {
        if (block_i >= num_blocks)
        {
            break;
        }

#define RENDER_MULTITHREADED 1
#if RENDER_MULTITHREADED
        TileRenderData data =
        {
            .block_start = block_i,
        };

        produce_render_work(milton_state, data);
        tile_acc += 1;
#else
        Arena tile_arena = arena_push(milton_state->transient_arena,
                                      arena_available_space(milton_state->transient_arena));
        render_tile(milton_state,
                    &tile_arena,
                    blocks,
                    block_i, num_blocks,
                    raster_buffer);


        arena_pop(&tile_arena);
#endif
        ARENA_VALIDATE(milton_state->transient_arena);
    }

#if RENDER_MULTITHREADED && 1
    // Wait for workers to finish.

    while(tile_acc)
    {
        i32 waited_err = SDL_SemWait(milton_state->render_queue->completed_semaphore);
        if (!waited_err)
        {
            --tile_acc;
        }
        else { assert ( !"Not handling completion semaphore wait error" ); }
    }
#endif

	for (int i = 0; i < milton_state->num_render_workers; ++i)
	{

		free(milton_state->render_worker_arenas[i].ptr);
		milton_state->render_worker_arenas[i] = (Arena){ 0 };
	}

    ARENA_VALIDATE(milton_state->transient_arena);

    return true;
}

func void render_picker(ColorPicker* picker,
                        u32* buffer_pixels, CanvasView* view)
{
    v4f background_color =
    {
        0.5f,
        0.5f,
        0.55f,
        0.4f,
    };
    background_color = to_premultiplied(background_color.rgb, background_color.a);

    Rect draw_rect = picker_get_bounds(picker);

    // Copy canvas buffer into picker buffer
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 picker_i =
                    (j - draw_rect.top) * (2*picker->bounds_radius_px ) + (i - draw_rect.left);
            u32 src = buffer_pixels[j * view->screen_size.w + i];
            picker->pixels[picker_i] = src;
        }
    }

    // Render background color.
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 picker_i =
                    (j - draw_rect.top) *( 2*picker->bounds_radius_px ) + (i - draw_rect.left);


            // TODO: Premultiplied?
            v4f dest = color_u32_to_v4f(picker->pixels[picker_i]);

            v4f result = blend_v4f(dest, background_color);

            picker->pixels[picker_i] = color_v4f_to_u32(result);
        }
    }

    rasterize_color_picker(picker, draw_rect);

    // Blit picker pixels
    u32* to_blit = picker->pixels;
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 linear_color = *to_blit++;
            v4f rgba = color_u32_to_v4f(linear_color);
            //sRGB = to_premultiplied(sRGB.rgb, sRGB.a);
            u32 color = color_v4f_to_u32(rgba);

            buffer_pixels[j * view->screen_size.w + i] = color;
        }
    }
}

typedef enum
{
    MiltonRenderFlags_NONE              = 0,
    MiltonRenderFlags_PICKER_UPDATED    = (1 << 0),
    MiltonRenderFlags_FULL_REDRAW       = (1 << 1),
} MiltonRenderFlags;

func void milton_render(MiltonState* milton_state, MiltonRenderFlags render_flags)
{
    // `raster_limits` is the part of the screen (in pixels) that should be updated
    // with what's on the canvas.
    Rect raster_limits = { 0 };

    // Figure out what `raster_limits` should be.
    {
        if (render_flags & MiltonRenderFlags_FULL_REDRAW)
        {
            raster_limits.left = 0;
            raster_limits.right = milton_state->view->screen_size.w;
            raster_limits.top = 0;
            raster_limits.bottom = milton_state->view->screen_size.h;
        }
        else if (milton_state->working_stroke.num_points > 1)
        {
            Stroke* stroke = &milton_state->working_stroke;
            v2i new_point = canvas_to_raster(
                    milton_state->view,
                    stroke->points[stroke->num_points - 2]);

            raster_limits.left   = min (milton_state->last_raster_input.x, new_point.x);
            raster_limits.right  = max (milton_state->last_raster_input.x, new_point.x);
            raster_limits.top    = min (milton_state->last_raster_input.y, new_point.y);
            raster_limits.bottom = max (milton_state->last_raster_input.y, new_point.y);
            i32 block_offset = 0;
            i32 w = raster_limits.right - raster_limits.left;
            i32 h = raster_limits.bottom - raster_limits.top;
            if (w < milton_state->block_width)
            {
                block_offset = (milton_state->block_width - w) / 2;
            }
            if (h < milton_state->block_width)
            {
                block_offset = max(block_offset, (milton_state->block_width - h) / 2);
            }
            raster_limits = rect_enlarge(raster_limits,
                    block_offset + (stroke->brush.radius / milton_state->view->scale));
            raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);
            assert (raster_limits.right >= raster_limits.left);
            assert (raster_limits.bottom >= raster_limits.top);
        }
        else if (milton_state->working_stroke.num_points == 1)
        {
            Stroke* stroke = &milton_state->working_stroke;
            v2i point = canvas_to_raster(milton_state->view, stroke->points[0]);
            i32 raster_radius = stroke->brush.radius / milton_state->view->scale;
            raster_radius = max(raster_radius, milton_state->block_width);
            raster_limits.left = -raster_radius  + point.x;
            raster_limits.right = raster_radius  + point.x;
            raster_limits.top = -raster_radius   + point.y;
            raster_limits.bottom = raster_radius + point.y;
            raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);
            assert (raster_limits.right >= raster_limits.left);
            assert (raster_limits.bottom >= raster_limits.top);
        }
    }

    //i32 index = (milton_state->raster_buffer_index + 1) % 2;
    i32 index = (milton_state->raster_buffer_index + 0);
    u32* raster_buffer = (u32*)milton_state->raster_buffers[index];

#if 0
    // DEBUG always render the whole thing.
    {
        raster_limits.left = 0;
        raster_limits.right = milton_state->view->screen_size.w;
        raster_limits.top = 0;
        raster_limits.bottom = milton_state->view->screen_size.h;
    }
#endif

    b32 completed = render_canvas(milton_state, raster_buffer, raster_limits);

    // Render UI
    if (completed)
    {
        b32 redraw = false;
        Rect picker_rect = picker_get_bounds(&milton_state->picker);
        Rect clipped = rect_intersect(picker_rect, raster_limits);
        if ((clipped.left != clipped.right) && clipped.top != clipped.bottom)
        {
            redraw = true;
        }

        if (redraw || (render_flags & MiltonRenderFlags_PICKER_UPDATED))
        {
            render_canvas(milton_state, raster_buffer, picker_rect);

            render_picker(&milton_state->picker,
                          raster_buffer,
                          milton_state->view);
        }
    }

    // If not preempted, do a buffer swap.
    if (completed)
    {
        i32 prev_index = milton_state->raster_buffer_index;
        milton_state->raster_buffer_index = index;

        memcpy(milton_state->raster_buffers[prev_index],
               milton_state->raster_buffers[index],
               milton_state->max_width * milton_state->max_height * milton_state->bytes_per_pixel);
    }
}
