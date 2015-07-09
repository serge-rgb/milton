// rasterizer.h
// (c) Copyright 2015 by Sergio Gonzalez


typedef struct ClippedStroke_s ClippedStroke;
struct ClippedStroke_s
{
    b32     fills_block;
    Brush   brush;
    i32     num_points;
    v2i     canvas_reference;
    v2i*    points;

    ClippedStroke* next;
};

static ClippedStroke* stroke_clip_to_rect(Arena* render_arena, Stroke* stroke, Rect rect)
{
    ClippedStroke* clipped_stroke = arena_alloc_elem(render_arena, ClippedStroke);
    {
        clipped_stroke->brush = stroke->brush;
        clipped_stroke->num_points = 0;
        clipped_stroke->points = arena_alloc_array(render_arena, stroke->num_points * 2, v2i);
    }
    if (stroke->num_points == 1)
    {
        if (is_inside_rect(rect, stroke->points[0]))
        {
            clipped_stroke->points[clipped_stroke->num_points++] = stroke->points[0];
        }
    }
    else
    {
        i32 num_points = stroke->num_points;
        for (i32 point_i = 0; point_i < num_points - 1; ++point_i)
        {
            v2i a = stroke->points[point_i];
            v2i b = stroke->points[point_i + 1];

            // Very conservative...
            b32 inside = !((a.x > rect.right && b.x > rect.right) ||
                              (a.x < rect.left && b.x < rect.left) ||
                              (a.y < rect.top && b.y < rect.top) ||
                              (a.y > rect.bottom && b.y > rect.bottom));

            // We can add the segment
            if (inside)
            {
                clipped_stroke->points[clipped_stroke->num_points++] = a;
                clipped_stroke->points[clipped_stroke->num_points++] = b;
            }
        }
    }
    return clipped_stroke;
}

inline v2i closest_point_in_segment(v2i a, v2i b,
                                    v2f ab, f32 ab_magnitude_squared,
                                    v2i canvas_point)
{
    v2i point;
    f32 mag_ab = sqrtf(ab_magnitude_squared);
    f32 d_x = ab.x / mag_ab;
    f32 d_y = ab.y / mag_ab;
    f32 ax_x = (f32)(canvas_point.x - a.x);
    f32 ax_y = (f32)(canvas_point.y - a.y);
    f32 disc = d_x * ax_x + d_y * ax_y;
    if (disc >= 0 && disc <= mag_ab)
    {
        point = (v2i)
        {
            (i32)(a.x + disc * d_x), (i32)(a.y + disc * d_y),
        };
    }
    else if (disc < 0)
    {
        point = a;
    }
    else
    {
        point = b;
    }
    return point;
}

// NOTE: takes clipped points.
inline b32 is_rect_filled_by_stroke(Rect rect, v2i reference_point,
                                    v2i* points, i32 num_points,
                                    Brush brush, CanvasView* view)
{
    // Perf note: With the current use, this is actually going to be zero.
    // Maybe turn this into an assert and avoid extra computations?
    v2i rect_center =
    {
        ((rect.left + rect.right) / 2) - reference_point.x,
        ((rect.top + rect.bottom) / 2) - reference_point.y,
    };

    if (num_points >= 2)
    {
        for (i32 point_i = 0; point_i < num_points; point_i += 2)
        {
            v2i a = points[point_i];
            v2i b = points[point_i + 1];

            a = sub_v2i(points[point_i], reference_point);
            b = sub_v2i(points[point_i + 1], reference_point);

            // Get closest point
            v2f ab = {(f32)(b.x - a.x), (f32)(b.y - a.y)};
            f32 mag_ab2 = ab.x * ab.x + ab.y * ab.y;
            v2i p  = closest_point_in_segment( a, b, ab, mag_ab2, rect_center);

            // Back to global coordinates
            p = add_v2i(p, reference_point);

            // Half width of a rectangle contained by brush at point p.
            i32 rad = (i32)(brush.radius * 0.707106781f);  // cos(pi/4)
            Rect bounded_rect;
            {
                bounded_rect.left   = p.x - rad;
                bounded_rect.right  = p.x + rad;
                bounded_rect.bottom = p.y + rad;
                bounded_rect.top    = p.y - rad;
            }

            if (is_rect_within_rect(rect, bounded_rect))
            {
                return true;
            }
        }
    }
    else if (num_points == 1)
    {
        v2i p  = points[0];

        // Half width of a rectangle contained by brush at point p.
        i32 rad = (i32)(brush.radius * 0.707106781f);  // cos(pi/4)
        Rect bounded_rect;
        {
            bounded_rect.left   = p.x - rad;
            bounded_rect.right  = p.x + rad;
            bounded_rect.bottom = p.y + rad;
            bounded_rect.top    = p.y - rad;
        }

        if (is_rect_within_rect(rect, bounded_rect))
        {
            return true;
        }
    }
    return false;
}

inline void render_canvas_in_block(Arena* render_arena,
                                   CanvasView* view,
                                   ColorManagement cm,
                                   Stroke* strokes,
                                   b32* stroke_masks,
                                   i32   num_strokes,
                                   Stroke* working_stroke,
                                   u32* pixels,
                                   Rect raster_limits)
{
    Rect canvas_limits;
    {
        canvas_limits.top_left = raster_to_canvas(view, raster_limits.top_left);
        canvas_limits.bot_right = raster_to_canvas(view, raster_limits.bot_right);
    }

    if (canvas_limits.left   < -view->canvas_tile_radius ||
        canvas_limits.right  > view->canvas_tile_radius  ||
        canvas_limits.top    < -view->canvas_tile_radius ||
        canvas_limits.bottom > view->canvas_tile_radius
        )
    {
        for (int j = raster_limits.top; j < raster_limits.bottom; j++)
        {
            for (int i = raster_limits.left; i < raster_limits.right; i++)
            {
                pixels[j * view->screen_size.w + i] = 0xffff00ff;
            }
        }
        return;
    }
    ClippedStroke* stroke_list = NULL;

    v2i reference_point =
// Leaving this toggle-able for a quick way to show the cool precision error.
#if 1
    {
        (canvas_limits.left + canvas_limits.right) / 2,
        (canvas_limits.top + canvas_limits.bottom) / 2,
    };
#else
    {0};
#endif

    // Go backwards so that list is in the correct older->newer order.
    for (int stroke_i = num_strokes; stroke_i >= 0; --stroke_i)
    {
        if (stroke_i < num_strokes && !stroke_masks[stroke_i])
        {
            continue;
        }
        Stroke* stroke = NULL;
        if (stroke_i == num_strokes)
        {
            stroke = working_stroke;
        }
        else
        {
            stroke = &strokes[stroke_i];
        }
        assert(stroke);
        Rect enlarged_limits = rect_enlarge(canvas_limits, stroke->brush.radius);
        ClippedStroke* clipped_stroke = stroke_clip_to_rect(render_arena, stroke, enlarged_limits);
        if (clipped_stroke->num_points)
        {
            ClippedStroke* list_head = clipped_stroke;
            list_head->next = stroke_list;
            if (is_rect_filled_by_stroke(
                        canvas_limits, reference_point,
                        clipped_stroke->points, clipped_stroke->num_points, clipped_stroke->brush,
                        view))
            {
                list_head->fills_block = true;
            }
            stroke_list = list_head;
        }
    }

    // Set our `stroke_list` to begin at the first opaque stroke that fills
    // this block.
    ClippedStroke* list_iter = stroke_list;
    while (list_iter)
    {
        if (list_iter->fills_block && list_iter->brush.alpha == 1.0f)
        {
            stroke_list = list_iter;
        }
        list_iter = list_iter->next;
    }

    int pixel_jump = view->downsampling_factor;  // Different names for the same thing.

    for (int j = raster_limits.top; j < raster_limits.bottom; j += pixel_jump)
    {
        for (int i = raster_limits.left; i < raster_limits.right; i += pixel_jump)
        {
            v2i raster_point = {i, j};
            v2i canvas_point = raster_to_canvas(view, raster_point);

            canvas_point = sub_v2i(canvas_point, reference_point);

            // Clear color
            f32 dr = 1.0f;
            f32 dg = 1.0f;
            f32 db = 1.0f;
            f32 da = 0.0f;

            ClippedStroke* list_iter = stroke_list;
            while(list_iter)
            {
                ClippedStroke* clipped_stroke = list_iter;
                /* v2i stroke_canvas_reference = clipped_stroke->canvas_reference; */
                list_iter = list_iter->next;

                assert (clipped_stroke);

                // Fast path.
                if (clipped_stroke->fills_block)
                {
#if 0  // Visualize it with black
                    f32 sr = clipped_stroke->brush.color.r * 0;
                    f32 sg = clipped_stroke->brush.color.g * 0;
                    f32 sb = clipped_stroke->brush.color.b * 0;
#else
                    f32 sr = clipped_stroke->brush.color.r;
                    f32 sg = clipped_stroke->brush.color.g;
                    f32 sb = clipped_stroke->brush.color.b;
#endif
                    f32 sa = clipped_stroke->brush.alpha;

                    // Move to gamma space
                    f32 g_dr = dr * dr;
                    f32 g_dg = dg * dg;
                    f32 g_db = db * db;
                    sr = sr * sr;
                    sg = sg * sg;
                    sb = sb * sb;

                    g_dr = (1 - sa) * g_dr + sa * sr;
                    g_dg = (1 - sa) * g_dg + sa * sg;
                    g_db = (1 - sa) * g_db + sa * sb;
                    da = sa + da * (1 - sa);

                    // Back to linear space
                    dr = sqrtf(g_dr);
                    dg = sqrtf(g_dg);
                    db = sqrtf(g_db);

                }
                // Slow path. There are pixels not inside.
                else
                {
                    v2i* points = clipped_stroke->points;

                    v2i min_point = {0};
                    f32 min_dist = FLT_MAX;
                    f32 dx = 0;
                    f32 dy = 0;
                    //int64 radius_squared = stroke->brush.radius * stroke->brush.radius;
                    if (clipped_stroke->num_points == 1)
                    {
                        min_point = sub_v2i(points[0], reference_point);
                        dx = (f32) (canvas_point.x - min_point.x);
                        dy = (f32) (canvas_point.y - min_point.y);
                        min_dist = dx * dx + dy * dy;
                    }
                    else
                    {
                        // Find closest point.
                        for (int point_i = 0; point_i < clipped_stroke->num_points-1; point_i += 2)
                        {
                            v2i a = points[point_i];
                            v2i b = points[point_i + 1];
                            a = sub_v2i(a, reference_point);
                            b = sub_v2i(b, reference_point);

                            v2f ab = {(f32)(b.x - a.x), (f32)(b.y - a.y)};
                            f32 mag_ab2 = ab.x * ab.x + ab.y * ab.y;
                            if (mag_ab2 > 0)
                            {
                                v2i point = closest_point_in_segment(a, b,
                                                                     ab, mag_ab2, canvas_point);

                                f32 test_dx = (f32) (canvas_point.x - point.x);
                                f32 test_dy = (f32) (canvas_point.y - point.y);
                                f32 dist = test_dx * test_dx + test_dy * test_dy;
                                if (dist < min_dist)
                                {
                                    min_dist = dist;
                                    min_point = point;
                                    dx = test_dx;
                                    dy = test_dy;
                                }
                            }
                        }
                    }

                    if (min_dist < FLT_MAX)
                    {
#define MSAA_4X 1
#if !MSAA_4X
#define MSAA_ROTATED_GRID
#endif

                        int samples = 0;
                        {
#ifdef MSAA_ROTATED_GRID
                            f32 u = (0.223607f * view->scale) * pixel_jump;  // sin(arctan(1/2)) / 2
                            f32 v = (0.670820f * view->scale) * pixel_jump;  // cos(arctan(1/2)) / 2 + u

                            f32 dists[4];
                            dists[0] = (dx - u) * (dx - u) + (dy - v) * (dy - v);
                            dists[1] = (dx - v) * (dx - v) + (dy + u) * (dy + u);
                            dists[2] = (dx + u) * (dx + u) + (dy + v) * (dy + v);
                            dists[3] = (dx + v) * (dx + v) + (dy + u) * (dy + u);
                            for (int i = 0; i < 4; ++i)
                            {
                                if (sqrtf(dists[i]) < clipped_stroke->brush.radius)
                                {
                                    ++samples;
                                }
                            }
#elif defined(MSAA_4X)
                            f32 dists[16];

                            f32 f3 = (0.75f * view->scale) * pixel_jump;
                            f32 f1 = (0.25f * view->scale) * pixel_jump;

                            dists[0]  = (dx - f3) * (dx - f3) + (dy - f3) * (dy - f3);
                            dists[1]  = (dx - f1) * (dx - f1) + (dy - f3) * (dy - f3);
                            dists[2]  = (dx + f1) * (dx + f1) + (dy - f3) * (dy - f3);
                            dists[3]  = (dx + f3) * (dx + f3) + (dy - f3) * (dy - f3);

                            dists[4]  = (dx - f3) * (dx - f3) + (dy - f1) * (dy - f1);
                            dists[5]  = (dx - f1) * (dx - f1) + (dy - f1) * (dy - f1);
                            dists[6]  = (dx + f1) * (dx + f1) + (dy - f1) * (dy - f1);
                            dists[7]  = (dx + f3) * (dx + f3) + (dy - f1) * (dy - f1);

                            dists[8]  = (dx - f3) * (dx - f3) + (dy + f1) * (dy + f1);
                            dists[9]  = (dx - f1) * (dx - f1) + (dy + f1) * (dy + f1);
                            dists[10] = (dx + f1) * (dx + f1) + (dy + f1) * (dy + f1);
                            dists[11] = (dx + f3) * (dx + f3) + (dy + f1) * (dy + f1);

                            dists[12] = (dx - f3) * (dx - f3) + (dy + f3) * (dy + f3);
                            dists[13] = (dx - f1) * (dx - f1) + (dy + f3) * (dy + f3);
                            dists[14] = (dx + f1) * (dx + f1) + (dy + f3) * (dy + f3);
                            dists[15] = (dx + f3) * (dx + f3) + (dy + f3) * (dy + f3);

                            /* i32 square_rad = */
                            /*         clipped_stroke->brush.radius * clipped_stroke->brush.radius; */

                            // Perf note: It would be nice to remove the sqrtf call , but
                            // we do get into precision errors at high zoom levels.

                            // We have to call a bunch of sqrtf's because squaring the radius
                            // woud overflow a 32-bit integer
                            if (clipped_stroke->brush.radius >= (1 << 16))
                            {
                                samples += (sqrtf(dists[ 0]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[ 1]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[ 2]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[ 3]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[ 4]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[ 5]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[ 6]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[ 7]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[ 8]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[ 9]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[10]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[11]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[12]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[13]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[14]) < clipped_stroke->brush.radius);
                                samples += (sqrtf(dists[15]) < clipped_stroke->brush.radius);
                            }
                            else
                            {
                                assert ( clipped_stroke->brush.radius > 0);

                                u32 sq_radius =
                                        (u32)clipped_stroke->brush.radius *
                                        (u32)clipped_stroke->brush.radius;
                                samples += (dists[ 0] < sq_radius);
                                samples += (dists[ 1] < sq_radius);
                                samples += (dists[ 2] < sq_radius);
                                samples += (dists[ 3] < sq_radius);
                                samples += (dists[ 4] < sq_radius);
                                samples += (dists[ 5] < sq_radius);
                                samples += (dists[ 6] < sq_radius);
                                samples += (dists[ 7] < sq_radius);
                                samples += (dists[ 8] < sq_radius);
                                samples += (dists[ 9] < sq_radius);
                                samples += (dists[10] < sq_radius);
                                samples += (dists[11] < sq_radius);
                                samples += (dists[12] < sq_radius);
                                samples += (dists[13] < sq_radius);
                                samples += (dists[14] < sq_radius);
                                samples += (dists[15] < sq_radius);
                            }

#endif
                        }

                        // If the stroke contributes to the pixel, do compositing.
                        if (samples > 0)
                        {
                            // Do compositing
                            // ---------------

#ifdef MSAA_ROTATED_GRID
                            f32 coverage = (f32)samples / 4.0f;
#elif defined(MSAA_4X)
                            f32 coverage = (f32)samples / 16.0f;
#endif

                            f32 sr = clipped_stroke->brush.color.r;
                            f32 sg = clipped_stroke->brush.color.g;
                            f32 sb = clipped_stroke->brush.color.b;
                            f32 sa = clipped_stroke->brush.alpha;

                            sa *= coverage;

                            // Move to gamma space
                            f32 g_dr = dr * dr;
                            f32 g_dg = dg * dg;
                            f32 g_db = db * db;
                            sr = sr * sr;
                            sg = sg * sg;
                            sb = sb * sb;

                            g_dr = (1 - sa) * g_dr + sa * sr;
                            g_dg = (1 - sa) * g_dg + sa * sg;
                            g_db = (1 - sa) * g_db + sa * sb;
                            da = sa + da * (1 - sa);

                            // Back to linear space
                            dr = sqrtf(g_dr);
                            dg = sqrtf(g_dg);
                            db = sqrtf(g_db);
                        }
                    }

                }
            }
            // From [0, 1] to [0, 255]
            v4f d = {
                dr, dg, db, da
            };
            u32 pixel = color_v4f_to_u32(cm, d);

            // TODO: Bilinear sampling could be nice here
            for (int jj = j; jj < j + pixel_jump; ++jj)
            {
                for (int ii = i; ii < i + pixel_jump; ++ii)
                {
                    pixels[jj * view->screen_size.w + ii] = pixel;
                }
            }
        }
    }
}

static void render_tile(MiltonState* milton_state,
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

    for (int block_i = 0; block_i < blocks_per_tile; ++block_i)
    {
        if (block_start + block_i >= num_blocks)
        {
            break;
        }

        render_canvas_in_block(tile_arena,
                               milton_state->view,
                               milton_state->cm,
                               milton_state->strokes,
                               stroke_masks,
                               milton_state->num_strokes,
                               &milton_state->working_stroke,
                               raster_buffer,
                               blocks[block_start + block_i]);
    }
}


// Render Workers:
//    We have a bunch of workers running on threads, who wait on a lockless
//    queue to take TileRenderData structures.
//    When there is work available, they call tile_render_thread with the
//    appropriate parameters.

typedef struct TileRenderData_s
{
    i32     block_start;
} TileRenderData;

#define RENDER_QUEUE_SIZE 1024

typedef struct RenderQueue_s
{
    Rect*   blocks;  // Screen areas to render.
    i32     num_blocks;
    u32*    raster_buffer;

    // FIFO work queue
    SDL_mutex*      mutex;
    TileRenderData  tile_render_data[RENDER_QUEUE_SIZE];
    i32    index;

    SDL_sem*    work_available;
    SDL_sem*    completed_semaphore;
} RenderQueue;

typedef struct
{
    MiltonState* milton_state;
    i32 worker_id;
} WorkerParams;

static void SDLCALL render_worker(void* data)
{
    WorkerParams* params = (WorkerParams*) data;
    MiltonState* milton_state = params->milton_state;
    i32 id = params->worker_id;
    RenderQueue* render_queue = milton_state->render_queue;

    for (;;)
    {
        SDL_SemWait(render_queue->work_available);
        i32 lock_err = SDL_LockMutex(render_queue->mutex);
        i32 index = -1;
        TileRenderData tile_data = { 0 };
        if (!lock_err)
        {
            index = --render_queue->index;
            assert (index >= 0);
            assert (index <  RENDER_QUEUE_SIZE);
            tile_data = render_queue->tile_render_data[index];
            SDL_UnlockMutex(render_queue->mutex);
        }
        else { assert (!"Render worker not handling lock error"); }

        assert (index >= 0);

        render_tile(milton_state,
                    &milton_state->render_worker_arenas[id],
                    render_queue->blocks,
                    tile_data.block_start, render_queue->num_blocks,
                    render_queue->raster_buffer);

        arena_reset(&milton_state->render_worker_arenas[id]);

        SDL_SemPost(render_queue->completed_semaphore);
    }
}

static void produce_render_work(MiltonState* milton_state, TileRenderData tile_render_data)
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


static b32 render_canvas(MiltonState* milton_state, u32* raster_buffer, Rect raster_limits)
{
    v2i canvas_reference = { 0 };
    Rect* blocks = NULL;
    i32 num_blocks = rect_split(milton_state->transient_arena,
            raster_limits, milton_state->block_width, milton_state->block_width, &blocks);

    RenderQueue* render_queue = milton_state->render_queue;
    {
        render_queue->blocks = blocks;
        render_queue->num_blocks = num_blocks;
        render_queue->raster_buffer = raster_buffer;
    }

    const i32 blocks_per_tile = milton_state->blocks_per_tile;

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

#if RENDER_MULTITHREADED
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

    ARENA_VALIDATE(milton_state->transient_arena);

    return true;
}

static void render_picker(ColorPicker* picker,
                          ColorManagement cm,
                          u32* buffer_pixels, CanvasView* view)
{
    v2f baseline = {1,0};

    v4f background_color =
    {
        0.5f,
        0.5f,
        0.55f,
        0.4f,
    };

    Rect draw_rect = picker_get_bounds(picker);

    // Copy canvas buffer into picker buffer
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 picker_i =
                    (j - draw_rect.top) * (2*picker->bound_radius_px ) + (i - draw_rect.left);
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
                    (j - draw_rect.top) *( 2*picker->bound_radius_px ) + (i - draw_rect.left);
            v4f dest = color_u32_to_v4f(cm, picker->pixels[picker_i]);
            f32 alpha = background_color.a;
            // To gamma
            background_color.r = background_color.r * background_color.r;
            background_color.g = background_color.g * background_color.g;
            background_color.b = background_color.b * background_color.b;
            dest.r = dest.r * dest.r;
            dest.g = dest.g * dest.g;
            dest.b = dest.b * dest.b;
            // Blend and move to linear
            v4f result =
            {
                sqrtf(dest.r * (1 - alpha) + background_color.r * alpha),
                sqrtf(dest.g * (1 - alpha) + background_color.g * alpha),
                sqrtf(dest.b * (1 - alpha) + background_color.b * alpha),
                dest.a + (alpha * (1 - dest.a)),
            };
            picker->pixels[picker_i] = color_v4f_to_u32(cm, result);
        }
    }

    // render wheel
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 picker_i =
                    (j - draw_rect.top) *( 2*picker->bound_radius_px ) + (i - draw_rect.left);
            v2f point = {(f32)i, (f32)j};
            u32 dest_color = picker->pixels[picker_i];

            int samples = 0;
            {
                f32 u = 0.223607f;
                f32 v = 0.670820f;

                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){-u, -v}));
                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){-v, u}));
                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){u, v}));
                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){v, u}));
            }

            if (samples > 0)
            {
                f32 angle = picker_wheel_get_angle(picker, point);
                f32 degree = radians_to_degrees(angle);
                v3f hsv = { degree, 1.0f, 1.0f };
                v3f rgb = hsv_to_rgb(hsv);

                f32 contrib = samples / 4.0f;

                v4f d = color_u32_to_v4f(cm, dest_color);

                // To gamma
                rgb.r = rgb.r * rgb.r;
                rgb.g = rgb.g * rgb.g;
                rgb.b = rgb.b * rgb.b;
                d.r = d.r * d.r;
                d.g = d.g * d.g;
                d.b = d.b * d.b;
                // Blend and move to linear
                v4f result =
                {
                    sqrtf(((1 - contrib) * (d.r)) + (contrib * (rgb.r))),
                    sqrtf(((1 - contrib) * (d.g)) + (contrib * (rgb.g))),
                    sqrtf(((1 - contrib) * (d.b)) + (contrib * (rgb.b))),
                    d.a + (contrib * (1 - d.a)),
                };
                u32 color = color_v4f_to_u32(cm, result);
                picker->pixels[picker_i] = color;
            }
        }
    }

    // Render triangle
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            v2f point = { (f32)i, (f32)j };
            u32 picker_i =
                    (j - draw_rect.top) *( 2*picker->bound_radius_px ) + (i - draw_rect.left);
            u32 dest_color = picker->pixels[picker_i];
            // MSAA!!
            int samples = 0;
            {
                f32 u = 0.223607f;
                f32 v = 0.670820f;

                samples += (int)is_inside_triangle(add_v2f(point, (v2f){-u, -v}),
                        picker->a, picker->b, picker->c);
                samples += (int)is_inside_triangle(add_v2f(point, (v2f){-v, u}),
                        picker->a, picker->b, picker->c);
                samples += (int)is_inside_triangle(add_v2f(point, (v2f){u, v}),
                        picker->a, picker->b, picker->c);
                samples += (int)is_inside_triangle(add_v2f(point, (v2f){v, u}),
                        picker->a, picker->b, picker->c);
            }

            if (samples > 0)
            {
                v3f hsv = picker_hsv_from_point(picker, point);

                f32 contrib = samples / 4.0f;

                v4f d = color_u32_to_v4f(cm, dest_color);

                v3f rgb = hsv_to_rgb(hsv);

                // To gamma
                rgb.r = rgb.r * rgb.r;
                rgb.g = rgb.g * rgb.g;
                rgb.b = rgb.b * rgb.b;
                d.r = d.r * d.r;
                d.g = d.g * d.g;
                d.b = d.b * d.b;
                // Blend and move to linear
                v4f result =
                {
                    sqrtf(((1 - contrib) * (d.r)) + (contrib * (rgb.r))),
                    sqrtf(((1 - contrib) * (d.g)) + (contrib * (rgb.g))),
                    sqrtf(((1 - contrib) * (d.b)) + (contrib * (rgb.b))),
                    d.a + (contrib * (1 - d.a)),
                };

                picker->pixels[picker_i] = color_v4f_to_u32(cm, result);
            }
        }
    }

    // Blit picker pixels
    u32* to_blit = picker->pixels;
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 linear_color = *to_blit++;
            v4f sRGB = color_u32_to_v4f(cm, linear_color);
            u32 color = color_v4f_to_u32(cm, sRGB);
            buffer_pixels[j * view->screen_size.w + i] = color;
        }
    }
}

typedef enum
{
    MiltonRenderFlags_none              = 0,
    MiltonRenderFlags_picker_updated    = (1 << 0),
    MiltonRenderFlags_full_redraw       = (1 << 1),
} MiltonRenderFlags;

static void milton_render(MiltonState* milton_state, MiltonRenderFlags render_flags)
{
    // `raster_limits` is the part of the screen (in pixels) that should be updated
    // with what's on the canvas.
    Rect raster_limits = { 0 };

    // Figure out what `raster_limits` should be.
    {
        if (render_flags & MiltonRenderFlags_full_redraw)
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
        Rect* split_rects = NULL;
        b32 redraw = false;
        Rect picker_rect = picker_get_bounds(&milton_state->picker);
        Rect clipped = rect_intersect(picker_rect, raster_limits);
        if ((clipped.left != clipped.right) && clipped.top != clipped.bottom)
        {
            redraw = true;
        }

        if (redraw || (render_flags & MiltonRenderFlags_picker_updated))
        {
            render_canvas(milton_state, raster_buffer, picker_rect);
            render_picker(&milton_state->picker, milton_state->cm,
                          raster_buffer,
                          milton_state->view);
        }
    }

    // If not preempted, do a buffer swap.
    if (false && completed)
    {
        i32 prev_index = milton_state->raster_buffer_index;
        milton_state->raster_buffer_index = index;

        memcpy(milton_state->raster_buffers[prev_index],
               milton_state->raster_buffers[index],
               milton_state->max_width * milton_state->max_height * milton_state->bytes_per_pixel);
    }
}
