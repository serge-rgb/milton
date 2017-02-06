// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "milton.h"

#include "common.h"
#include "color.h"
#include "canvas.h"
#include "gui.h"
#include "hardware_renderer.h"
#include "localization.h"
#include "persist.h"
#include "platform.h"
#include "vector.h"

// Defined below.
static void milton_validate(MiltonState* milton_state);

void
milton_set_background_color(MiltonState* milton_state, v3f background_color)
{
    milton_state->view->background_color = background_color;
    gpu_update_background(milton_state->render_data, background_color);
}

static void
milton_set_default_view(MiltonState* milton_state)
{
    CanvasView* view = milton_state->view;

    auto saved_size = view->screen_size;

    *view = CanvasView{};

    view->screen_size         = saved_size;
    view->zoom_center         = divide2i(saved_size, 2);
    view->scale               = MILTON_DEFAULT_SCALE;
    view->downsampling_factor = 1;
    view->num_layers          = 1;
    view->canvas_radius_limit = 1 << 30;  // A higher limit and certain assumptions start to break
}

static void
milton_update_brushes(MiltonState* milton_state)
{
    for ( int i = 0; i < BrushEnum_COUNT; ++i ) {
        Brush* brush = &milton_state->brushes[i];
        i32 size = milton_state->brush_sizes[i];
        brush->radius = size * milton_state->view->scale;
        mlt_assert(brush->radius < FLT_MAX);
        if ( i == BrushEnum_PEN ) {
            // Alpha is set by the UI
            brush->color = to_premultiplied(gui_get_picker_rgb(milton_state->gui), brush->alpha);

            // Check for eraser magic value (see blend.f.glsl and layer_blend.f.glsl)
            if ( brush->color.r == 0.0f &&
                 brush->color.g == 1.0f &&
                 brush->color.b == 0.0f &&
                 brush->color.a == 1.0f ) {
                brush->color.g = 0xFE/255.0f;
            }
        }
        else if ( i == BrushEnum_ERASER ) {
            brush->color = k_eraser_color;
        }
    }

    milton_state->working_stroke.brush = milton_state->brushes[BrushEnum_PEN];
}

static Brush
milton_get_brush(MiltonState* milton_state)
{
    Brush brush = { 0 };
    if ( milton_state->current_mode == MiltonMode_PEN ) {
        brush = milton_state->brushes[BrushEnum_PEN];
    }
    else if ( milton_state->current_mode == MiltonMode_ERASER ) {
        brush = milton_state->brushes[BrushEnum_ERASER];
    }
    return brush;
}

static i32*
pointer_to_brush_size(MiltonState* milton_state)
{
    i32* ptr = NULL;

    if ( milton_state->current_mode == MiltonMode_PEN ) {
        ptr = &milton_state->brush_sizes[BrushEnum_PEN];
    }
    else if ( milton_state->current_mode == MiltonMode_ERASER ) {
        ptr = &milton_state->brush_sizes[BrushEnum_ERASER];
    }
    return ptr;
}

static b32
is_user_drawing(MiltonState* milton_state)
{
    b32 result = milton_state->working_stroke.num_points > 0;
    return result;
}

static b32
current_mode_is_for_painting(MiltonState* milton_state)
{
    b32 result = milton_state->current_mode == MiltonMode_PEN || milton_state->current_mode == MiltonMode_ERASER;
    return result;
}

static void
clear_stroke_redo(MiltonState* milton_state)
{
    while ( milton_state->canvas->stroke_graveyard.count > 0 ) {
        Stroke s = pop(&milton_state->canvas->stroke_graveyard);
    }
    for ( i64 i = 0; i < milton_state->canvas->redo_stack.count; ++i ) {
        HistoryElement h = milton_state->canvas->redo_stack.data[i];
        if ( h.type == HistoryElement_STROKE_ADD ) {
            for ( i64 j = i; j < milton_state->canvas->redo_stack.count-1; ++j ) {
                milton_state->canvas->redo_stack.data[j] = milton_state->canvas->redo_stack.data[j+1];
            }
            pop(&milton_state->canvas->redo_stack);
        }
    }
}

static void
milton_stroke_input(MiltonState* milton_state, MiltonInput* input)
{
    i64 num_discarded = 0;
    if ( input->input_count == 0 ) {
        return;
    }

    Stroke* ws = &milton_state->working_stroke;

    //milton_log("Stroke input with %d packets\n", input->input_count);
    ws->brush    = milton_get_brush(milton_state);
    ws->layer_id = milton_state->view->working_layer_id;

    for ( int input_i = 0; input_i < input->input_count; ++input_i ) {
        v2i in_point = input->points[input_i];

        v2i canvas_point = raster_to_canvas(milton_state->view, in_point);

        f32 pressure = NO_PRESSURE_INFO;

        if ( input->pressures[input_i] != NO_PRESSURE_INFO ) {
            f32 pressure_min = 0.01f;
            pressure = pressure_min + input->pressures[input_i] * (1.0f - pressure_min);
        } else {
            pressure = 1.0f;
        }

        b32 not_the_first = false;
        if ( ws->num_points >= 1 ) {
            not_the_first = true;
        }

        // A point passes inspection if:
        //  a) it's the first point of this stroke
        //  b) it is being appended to the stroke and it didn't merge with the previous point.
        b32 passed_inspection = true;

        if ( pressure == NO_PRESSURE_INFO ) {
            passed_inspection = false;
            num_discarded++;
        }

        if ( passed_inspection && not_the_first ) {
            i32 in_radius = (i32)(pressure * ws->brush.radius);

            // Limit the number of points we check so that we don't mess with the stroke too much.
            int point_window = 4;
            int count = 0;
            // Pop every point that is contained by the new one, but don't leave it empty

            for ( i32 i = ws->num_points - 1; passed_inspection && i >= 0; --i ) {
                if ( ++count >= point_window ) {
                    break;
                }
                v2i this_point = ws->points[i];
                i32 this_radius = (i32)(ws->brush.radius * ws->pressures[i]);

                if ( stroke_point_contains_point(canvas_point, in_radius, this_point, this_radius) ) {
                    if ( ws->num_points > 1 ) {
                        --ws->num_points;
                    } else {
                        break;
                    }
                }
                else if ( stroke_point_contains_point(this_point, this_radius, canvas_point, in_radius) ) {
                    // If some other point in the past contains this point,
                    // then this point is invalid.
                    passed_inspection = false;
                    num_discarded += ws->num_points - i;
                    break;
                }
            }
        }

        // Cleared to be appended.
        if ( passed_inspection && ws->num_points < STROKE_MAX_POINTS ) {
            if ( milton_brush_smoothing_enabled(milton_state) ) {
                // Stroke smoothing.
                // Change canvas_point depending on the average of the last `N` points.
                // The new point is a weighted sum of factor*average (1-factor)*canvas_point
                i64 N = 2;
                if ( ws->num_points > N ) {
                    v2i average = {};
                    float factor = 0.55f;

                    for ( i64 i = 0; i < N; ++i ) {
                        average.x += ws->points[ws->num_points-1 - i].x / N;
                        average.y += ws->points[ws->num_points-1 - i].y / N;
                    }

                    auto* view = milton_state->view;

                    auto canvas_center = raster_to_canvas(view, divide2i(view->screen_size, 2));

                    float f_average_x = average.x - canvas_center.x;
                    float f_average_y = average.y - canvas_center.y;

                    float f_canvas_point_x = canvas_point.x - canvas_center.x;
                    float f_canvas_point_y = canvas_point.y - canvas_center.y;

                    canvas_point.x = (i32)roundf
                            (f_average_x*factor +
                             f_canvas_point_x*(1-factor));
                    canvas_point.y = (i32)roundf
                            (f_average_y*factor +
                             f_canvas_point_y*(1-factor));

                    canvas_point.x += canvas_center.x;
                    canvas_point.y += canvas_center.y;
                }
            }

            // Add to current stroke.
            int index = ws->num_points++;
            ws->points[index] = canvas_point;
            ws->pressures[index] = pressure;
        }
    }

#if 0
    if (num_discarded > 0)
    {
        milton_log("INFO: Discarded %d points.\n", num_discarded);
    }
#endif

    // Validate. remove points that are standing in the same place, even if they have different pressures.
    for ( i32 np = 0; np < ws->num_points-1; ++np ) {
        if ( equ2i(ws->points[np], ws->points[np+1])) {
            for ( i32 new_i = np; new_i < ws->num_points-1; ++new_i ) {
                ws->points[new_i] = ws->points[new_i+1];
            }
            --ws->num_points;
        }
    }
}

void
milton_set_zoom_at_point(MiltonState* milton_state, v2i zoom_center)
{
    milton_state->view->pan_vector = sub2i(milton_state->view->pan_vector,
                                           scale2i(sub2i(zoom_center, milton_state->view->zoom_center),
                                                   milton_state->view->scale));

    milton_state->view->zoom_center = zoom_center;
    gpu_update_canvas(milton_state->render_data, milton_state->view);
}

void
milton_set_zoom_at_screen_center(MiltonState* milton_state)
{
    milton_set_zoom_at_point(milton_state, divide2i(milton_state->view->screen_size, 2));
}

void
milton_set_canvas_file_(MiltonState* milton_state, PATH_CHAR* fname, b32 is_default)
{
    if ( is_default ) {
        milton_state->flags |= MiltonStateFlags_DEFAULT_CANVAS;
    } else {
        milton_state->flags &= ~MiltonStateFlags_DEFAULT_CANVAS;
    }

    u64 len = PATH_STRLEN(fname);
    if ( len > MAX_PATH ) {
        milton_log("milton_set_canvas_file: fname was too long %lu\n", len);
        fname = TO_PATH_STR("MiltonPersist.mlt");
    }
    milton_state->mlt_file_path = fname;

    if ( !is_default ) {
        milton_set_last_canvas_fname(fname);
    } else {
        milton_unset_last_canvas_fname();
    }
}

void
milton_set_canvas_file(MiltonState* milton_state, PATH_CHAR* fname)
{
    milton_set_canvas_file_(milton_state, fname, false);
}

// Helper function
void
milton_set_default_canvas_file(MiltonState* milton_state)
{
    PATH_CHAR* f = (PATH_CHAR*)mlt_calloc(MAX_PATH, sizeof(PATH_CHAR), "Strings");

    PATH_STRNCPY(f, TO_PATH_STR("MiltonPersist.mlt"), MAX_PATH);
    platform_fname_at_config(f, MAX_PATH);
    milton_set_canvas_file_(milton_state, f, true);
    milton_state->flags |= MiltonStateFlags_DEFAULT_CANVAS;
}

i32
milton_get_brush_radius(MiltonState* milton_state)
{
    i32 brush_size = 0;
    if ( milton_state->current_mode == MiltonMode_PEN ) {
        brush_size = milton_state->brush_sizes[BrushEnum_PEN];
    }
    else if ( milton_state->current_mode == MiltonMode_ERASER ) {
        brush_size = milton_state->brush_sizes[BrushEnum_ERASER];
    }
    return brush_size;
}

void
milton_set_brush_size(MiltonState* milton_state, i32 size)
{
    if ( current_mode_is_for_painting(milton_state) ) {
        if ( size < MILTON_MAX_BRUSH_SIZE && size > 0 ) {
            (*pointer_to_brush_size(milton_state)) = size;
            milton_update_brushes(milton_state);
        }
    }
}

// For keyboard shortcut.
void
milton_increase_brush_size(MiltonState* milton_state)
{
    if ( current_mode_is_for_painting(milton_state) ) {
        i32 brush_size = milton_get_brush_radius(milton_state);
        if ( brush_size < MILTON_MAX_BRUSH_SIZE && brush_size > 0 ) {
            milton_set_brush_size(milton_state, brush_size + 1);
        }
        milton_update_brushes(milton_state);
    }
}

// For keyboard shortcut.
void
milton_decrease_brush_size(MiltonState* milton_state)
{
    if ( current_mode_is_for_painting(milton_state) ) {
        i32 brush_size = milton_get_brush_radius(milton_state);
        if ( brush_size > 1 ) {
            milton_set_brush_size(milton_state, brush_size - 1);
        }
        milton_update_brushes(milton_state);
    }
}

void
milton_set_pen_alpha(MiltonState* milton_state, float alpha)
{
    milton_state->brushes[BrushEnum_PEN].alpha = alpha;
    milton_update_brushes(milton_state);
}

float
milton_get_pen_alpha(MiltonState* milton_state)
{
    const float alpha = milton_state->brushes[BrushEnum_PEN].alpha;
    return alpha;
}

void
milton_init(MiltonState* milton_state, i32 width, i32 height)
{
    init_localization();

    milton_state->canvas = arena_bootstrap(CanvasState, arena, 1024*1024);
    milton_state->working_stroke.points    = arena_alloc_array(&milton_state->root_arena, STROKE_MAX_POINTS, v2i);
    milton_state->working_stroke.pressures = arena_alloc_array(&milton_state->root_arena, STROKE_MAX_POINTS, f32);

#if SOFTWARE_RENDERER_COMPILED
    // Initialize render queue
    milton_state->render_stack = arena_alloc_elem(milton_state->root_arena, RenderStack);
    {
        milton_state->render_stack->work_available      = SDL_CreateSemaphore(0);
        milton_state->render_stack->completed_semaphore = SDL_CreateSemaphore(0);
        milton_state->render_stack->mutex               = SDL_CreateMutex();
        milton_state->num_render_workers = min(SDL_GetCPUCount(), MAX_NUM_WORKERS);
    }

#if RESTRICT_NUM_WORKERS_TO_2
    milton_state->num_render_workers = min(2, SDL_GetCPUCount());
#endif

    milton_log("[DEBUG]: Creating %d render workers.\n", milton_state->num_render_workers);

    milton_state->render_worker_arenas = arena_alloc_array(milton_state->root_arena,
                                                           milton_state->num_render_workers,
                                                           Arena);
    milton_state->worker_memory_size = 65536;


    mlt_assert (milton_state->num_render_workers);
    // Make the working stroke visible
    for ( int wi=0; wi< milton_state->num_render_workers; ++wi ) {
        milton_state->working_stroke.visibility[wi] = true;
    }


#endif //SOFTWARE_RENDERER_COMPILED

#if MILTON_SAVE_ASYNC
    milton_state->save_mutex = SDL_CreateMutex();
    milton_state->save_flag = SaveEnum_GOOD_TO_GO;
#endif

    milton_state->bytes_per_pixel = 4;


    milton_state->current_mode = MiltonMode_PEN;
    milton_state->last_mode = MiltonMode_NONE;


    milton_state->gl = arena_alloc_elem(&milton_state->root_arena, MiltonGLState);

#if 1
    milton_state->blocks_per_blockgroup = 16;
    milton_state->block_width = 32;
#else
    milton_state->blocks_per_blockgroup = 4;
    milton_state->block_width = 32;
#endif

    // TODO: Do a bootstrap here.
    milton_state->gui = arena_alloc_elem(&milton_state->root_arena, MiltonGui);
    gui_init(&milton_state->root_arena, milton_state->gui);


    milton_state->view = arena_alloc_elem(&milton_state->root_arena, CanvasView);
    milton_set_default_view(milton_state);

    milton_state->view->screen_size = { width, height };

    {
        i32 flags = RenderDataFlags_NONE;
        if ( milton_state->gui->visible ) {
            flags |= RenderDataFlags_GUI_VISIBLE;
        }
        gpu_init(milton_state->render_data, milton_state->view, &milton_state->gui->picker, flags);
    }

    milton_set_background_color(milton_state, v3f{ 1, 1, 1 });

    { // Get/Set Milton Canvas (.mlt) file
        PATH_CHAR* last_fname = milton_get_last_canvas_fname();

        if ( last_fname != NULL ) {
            milton_set_canvas_file(milton_state, last_fname);
        } else {
            milton_set_default_canvas_file(milton_state);
        }
    }

    // Set default brush sizes.
    for ( int i = 0; i < BrushEnum_COUNT; ++i ) {
        switch ( i ) {
        case BrushEnum_PEN:
            milton_state->brush_sizes[i] = 10;
            break;
        case BrushEnum_ERASER:
            milton_state->brush_sizes[i] = 40;
            break;
        default:
            INVALID_CODE_PATH;
            break;
        }
    }
    milton_set_pen_alpha(milton_state, 1.0f);

    milton_state->mlt_binary_version = 2;
    milton_state->last_save_time = {};
    // Note: This will fill out uninitialized data like default layers.
    milton_load(milton_state);
    milton_validate(milton_state);


    // Enable brush smoothing by default
    if ( !milton_brush_smoothing_enabled(milton_state) ) {
        milton_toggle_brush_smoothing(milton_state);
    }

#if MILTON_ENABLE_PROFILING
    milton_state->viz_window_visible = true;
#endif

#if SOFTWARE_RENDERER_COMPILED
    for ( i32 i = 0; i < milton_state->num_render_workers; ++i ) {
        WorkerParams* params = arena_alloc_elem(milton_state->root_arena, WorkerParams);
        {
            params->milton_state = milton_state;
            params->worker_id = i;
        }
        mlt_assert (milton_state->render_worker_arenas[i].ptr == NULL);
        u8* worker_memory = (u8*)mlt_calloc(1, milton_state->worker_memory_size);
        if ( !worker_memory ) {
            milton_die_gracefully("Platform allocation failed");
        }
        milton_state->render_worker_arenas[i] = arena_init(worker_memory,
                                                           milton_state->worker_memory_size);

        SDL_CreateThread(renderer_worker_thread, "Milton Render Worker", (void*)params);
    }
#endif

    milton_state->flags |= MiltonStateFlags_RUNNING;

#if MILTON_ENABLE_PROFILING
    profiler_init();
#endif


}

void
upload_gui(MiltonState* milton_state)
{
    gpu_update_canvas(milton_state->render_data, milton_state->view);
    gpu_resize(milton_state->render_data, milton_state->view);
    gpu_update_picker(milton_state->render_data, &milton_state->gui->picker);
}

// Returns false if the pan_delta moves the pan vector outside of the canvas.
b32
milton_resize_and_pan(MiltonState* milton_state, v2i pan_delta, v2i new_screen_size)
{
    b32 pan_ok = true;
    if ( (new_screen_size.w > 8000
          || new_screen_size.h > 8000
          || new_screen_size.w <= 0
          || new_screen_size.h <= 0) ) {
        return pan_ok;
    }

    b32 do_realloc = false;
    if ( milton_state->max_width <= new_screen_size.w ) {
        milton_state->max_width = new_screen_size.w + 256;
        do_realloc = true;
    }
    if ( milton_state->max_height <= new_screen_size.h ) {
        milton_state->max_height = new_screen_size.h + 256;
        do_realloc = true;
    }

    size_t buffer_size = (size_t) milton_state->max_width * milton_state->max_height * milton_state->bytes_per_pixel;

    #if SOFTWARE_RENDERER_COMPILED
    if ( do_realloc ) {
        u8* raster_buffer = milton_state->raster_buffer;
        u8* canvas_buffer = milton_state->canvas_buffer;
        if ( raster_buffer ) { mlt_free(raster_buffer, "Bitmap"); milton_state->raster_buffer = NULL; }
        if ( canvas_buffer ) { mlt_free(canvas_buffer, "Bitmap"); milton_state->canvas_buffer = NULL; }
        milton_state->raster_buffer      = (u8*)mlt_calloc(1, buffer_size, "Bitmap");
        milton_state->canvas_buffer      = (u8*)mlt_calloc(1, buffer_size, "Bitmap");

        if ( milton_state->raster_buffer == NULL ) {
            milton_die_gracefully("Could not allocate enough memory for raster buffer.");
        }
        if ( milton_state->canvas_buffer == NULL ) {
            milton_die_gracefully("Could not allocate enough memory for canvas buffer.");
        }
    }
    #endif

    if ( new_screen_size.w < milton_state->max_width && new_screen_size.h < milton_state->max_height ) {
        milton_state->view->screen_size = new_screen_size;

        // Add delta to pan vector
        v2i pan_vector = add2i(milton_state->view->pan_vector,
                               scale2i(pan_delta, milton_state->view->scale));

        if ( pan_vector.x > milton_state->view->canvas_radius_limit
             || pan_vector.x <= -milton_state->view->canvas_radius_limit ) {
            pan_vector.x = milton_state->view->pan_vector.x;
            pan_ok = false;
        }
        if ( pan_vector.y > milton_state->view->canvas_radius_limit
             || pan_vector.y <= -milton_state->view->canvas_radius_limit ) {
            pan_vector.y = milton_state->view->pan_vector.y;
            pan_ok = false;
        }
        milton_state->view->pan_vector = pan_vector;

        upload_gui(milton_state);
    } else {
        milton_die_gracefully("Fatal error. Screen size is more than Milton can handle.");
    }

    return pan_ok;
}

void
milton_reset_canvas(MiltonState* milton_state)
{
    CanvasState* canvas = milton_state->canvas;

    gpu_free_strokes(milton_state);
    milton_state->mlt_binary_version = MILTON_MINOR_VERSION;
    milton_state->last_save_time = {};

    // Clear history
    // TODO: These arrays should use the arena.
    release(&canvas->history);
    release(&canvas->redo_stack);
    release(&canvas->stroke_graveyard);

    size_t size = canvas->arena.min_block_size;
    arena_free(&canvas->arena);  // Note: This destroys the canvas
    milton_state->canvas = arena_bootstrap(CanvasState, arena, size);

    mlt_assert(milton_state->canvas->history.count == 0);
}

void
milton_reset_canvas_and_set_default(MiltonState* milton_state)
{
    milton_reset_canvas(milton_state);

    // New Root
    milton_new_layer(milton_state);

    // New View
    milton_set_default_view(milton_state);
    milton_state->view->background_color = {1,1,1};
    gpu_update_background(milton_state->render_data, milton_state->view->background_color);

    // Reset color buttons
    for ( ColorButton* b = milton_state->gui->picker.color_buttons; b!=NULL; b=b->next ) {
        b->rgba = {};
    }

    // gui init
    {
        MiltonGui* gui = milton_state->gui;
        gui->picker.data.hsv = { 0.0f, 1.0f, 0.7f };
        gui->visible = true;

        picker_init(&gui->picker);

        gui->preview_pos      = v2i{-1, -1};
        gui->preview_pos_prev = v2i{-1, -1};

        exporter_init(&gui->exporter);
    }
    milton_update_brushes(milton_state);

    milton_set_default_canvas_file(milton_state);
    upload_gui(milton_state);
}

void
milton_switch_mode(MiltonState* milton_state, MiltonMode mode)
{
    if ( mode != milton_state->current_mode ) {
        milton_state->last_mode = milton_state->current_mode;
        milton_state->current_mode = mode;

        if ( milton_state->last_mode == MiltonMode_EYEDROPPER &&
             milton_state->eyedropper_buffer != NULL ) {
            mlt_free(milton_state->eyedropper_buffer, "Bitmap");
            milton_state->eyedropper_buffer = NULL;
        }

        if ( mode == MiltonMode_EXPORTING && milton_state->gui->visible ) {
            gui_toggle_visibility(milton_state);
        }
        if ( milton_state->last_mode == MiltonMode_EXPORTING && !milton_state->gui->visible ) {
            gui_toggle_visibility(milton_state);
        }
    }
}

void
milton_use_previous_mode(MiltonState* milton_state)
{
    if ( milton_state->last_mode != MiltonMode_NONE ) {
        milton_switch_mode(milton_state, milton_state->last_mode);
    } else {
        INVALID_CODE_PATH;
    }
}

void
milton_try_quit(MiltonState* milton_state)
{
    milton_state->flags &= ~MiltonStateFlags_RUNNING;
}

void
milton_expand_render_memory(MiltonState* milton_state)
{
#if SOFTWARE_RENDERER_COMPILED
    if ( milton_state->flags & MiltonStateFlags_WORKER_NEEDS_MEMORY ) {
        size_t prev_memory_value = milton_state->worker_memory_size;
        milton_state->worker_memory_size *= 2;
        size_t needed_size = milton_state->worker_memory_size;

        for ( int i = 0; i < milton_state->num_render_workers; ++i ) {
            if ( milton_state->render_worker_arenas[i].ptr != NULL ) {
                mlt_free(milton_state->render_worker_arenas[i].ptr);
            }
            u8* new_memory = (u8*)mlt_calloc(1, needed_size);
            milton_state->render_worker_arenas[i] = arena_init(new_memory, needed_size);
            if ( milton_state->render_worker_arenas[i].ptr == NULL ) {
                milton_die_gracefully("Failed to realloc worker arena\n");
            }
        }

        milton_log("[DEBUG] Assigning more memory per worker. From %lu to %lu\n",
                   prev_memory_value,
                   milton_state->worker_memory_size);

        milton_state->flags &= ~MiltonStateFlags_WORKER_NEEDS_MEMORY;
    }
#endif  // SOFTWARE_RENDERER_COMPILED
}

void
milton_save_postlude(MiltonState* milton_state)
{
    milton_state->last_save_time = platform_get_walltime();
    milton_state->last_save_stroke_count = count_strokes(milton_state->canvas->root_layer);

    milton_state->flags &= ~MiltonStateFlags_LAST_SAVE_FAILED;
}

#if MILTON_SAVE_ASYNC
int  // Thread
milton_save_async(void* state_)
{
    MiltonState* milton_state = state_;

    SDL_LockMutex(milton_state->save_mutex);
    i64 flag = milton_state->save_flag;

    if( flag == SaveEnum_GOOD_TO_GO ) {
        milton_state->save_flag = SaveEnum_IN_USE;
        SDL_UnlockMutex(milton_state->save_mutex);

        milton_save(milton_state);

        SDL_Delay(2000);  // Overkill to save more than every two seconds!.

        SDL_LockMutex(milton_state->save_mutex);
        milton_state->save_flag = SaveEnum_GOOD_TO_GO;
        SDL_UnlockMutex(milton_state->save_mutex);
    }
    else if ( flag == SaveEnum_IN_USE ) {
        SDL_UnlockMutex(milton_state->save_mutex);
    }

    return flag;
}
#endif

void
milton_new_layer(MiltonState* milton_state)
{
    CanvasState* canvas = milton_state->canvas;
    i32 id = canvas->layer_guid++;
    milton_log("Increased guid to %d\n", canvas->layer_guid);

    Layer* layer = arena_alloc_elem(&canvas->arena, Layer);
    {
        layer->id = id;
        layer->flags = LayerFlags_VISIBLE;
        layer->strokes.arena = &canvas->arena;
        layer->alpha = 1.0f;
        strokelist_init_bucket(&layer->strokes.root);
    }
    snprintf(layer->name, 1024, "Layer %d", layer->id);

    if ( canvas->root_layer != NULL ) {
        Layer* top = layer_get_topmost(canvas->root_layer);
        top->next = layer;
        layer->prev = top;
        milton_set_working_layer(milton_state, top->next);
    } else {
        canvas->root_layer = layer;
        milton_set_working_layer(milton_state, layer);
    }
}

void
milton_set_working_layer(MiltonState* milton_state, Layer* layer)
{
    milton_state->canvas->working_layer = layer;
    milton_state->view->working_layer_id = layer->id;
}

void
milton_delete_working_layer(MiltonState* milton_state)
{
    Layer* layer = milton_state->canvas->working_layer;
    if ( layer->next || layer->prev ) {
        if (layer->next) layer->next->prev = layer->prev;
        if (layer->prev) layer->prev->next = layer->next;

        Layer* wl = NULL;
        if (layer->next) wl = layer->next;
        else wl = layer->prev;
        milton_set_working_layer(milton_state, wl);
    }
    if ( layer == milton_state->canvas->root_layer )
        milton_state->canvas->root_layer = milton_state->canvas->working_layer;

    milton_state->flags |= MiltonStateFlags_REQUEST_QUALITY_REDRAW;
}

b32
milton_brush_smoothing_enabled(MiltonState* milton_state)
{
    b32 enabled = (milton_state->flags & MiltonStateFlags_BRUSH_SMOOTHING);
    return enabled;
}

void
milton_toggle_brush_smoothing(MiltonState* milton_state)
{
    if ( milton_brush_smoothing_enabled(milton_state) ) {
        milton_state->flags &= ~MiltonStateFlags_BRUSH_SMOOTHING;
    } else {
        milton_state->flags |= MiltonStateFlags_BRUSH_SMOOTHING;
    }
}

static void
milton_validate(MiltonState* milton_state)
{
    // Make sure that the history reflects the strokes that exist
    i64 num_layers=0;
    for ( Layer* l = milton_state->canvas->root_layer; l != NULL; l = l->next ) {
        ++num_layers;
    }
    i32* layer_ids = (i32*)mlt_calloc((size_t)num_layers, sizeof(i32), "Validate");
    i64 i = 0;
    for ( Layer* l = milton_state->canvas->root_layer; l != NULL; l = l->next ) {
        layer_ids[i] = l->id;
        ++i;
    }

    i64 history_count = 0;
    for ( i64 hi = 0; hi < milton_state->canvas->history.count; ++hi ) {
        i32 id = milton_state->canvas->history.data[hi].layer_id;
        for ( i64 li = 0; li < num_layers; ++li ) {
            if ( id == layer_ids[li] ) {
                ++history_count;
            }
        }
    }

    i64 stroke_count = count_strokes(milton_state->canvas->root_layer);
    if ( history_count != stroke_count ) {
        milton_log("WARNING: Recreating history. File says History: %d(max %d) Actual strokes: %d\n",
                   history_count, milton_state->canvas->history.count,
                   stroke_count);
        reset(&milton_state->canvas->history);
        i32 id = 0;
        for ( Layer *l = milton_state->canvas->root_layer;
              l != NULL;
              l = l->next ) {
            for ( i64 si = 0; si < l->strokes.count; ++si ) {
                Stroke* s = get(&l->strokes, si);
                HistoryElement he = { HistoryElement_STROKE_ADD, s->layer_id };
                push(&milton_state->canvas->history, he);
            }
        }
    }

    mlt_free(layer_ids, "Validate");
}

// Copy points from in_stroke to out_stroke, but do interpolation to smooth it out.
static void
copy_with_smooth_interpolation(Arena* arena, CanvasView* view, Stroke* in_stroke, Stroke* out_stroke)
{
    i32 num_points = in_stroke->num_points;

    // At most we are adding twice as many points. This is wasteful but at the moment it looks like
    // a reasonable tradeoff vs the complexity/perf hit of using something smaller.

    if ( num_points >= 4 && 2*num_points <= STROKE_MAX_POINTS ) {
        out_stroke->points    = arena_alloc_array(arena, 2*num_points, v2i);
        out_stroke->pressures = arena_alloc_array(arena, 2*num_points, f32);

        // Push the first points.
        memcpy(out_stroke->points, in_stroke->points, 4 * sizeof(v2i));
        memcpy(out_stroke->pressures, in_stroke->pressures, 4 * sizeof(f32));


        i32 out_i = 4;

        // Go through each four consecutive points in the stroke and copy them to new array, but do
        // interpolation with a cubic Bezier.

        // Relative to center to maintain precision.
        v2i canvas_center = raster_to_canvas(view, divide2i(view->screen_size, 2));
        v2f a = {}; // Will get copied from b in the loop below.
        v2f b = v2i_to_v2f(sub2i(in_stroke->points[0], canvas_center));
        v2f c = v2i_to_v2f(sub2i(in_stroke->points[1], canvas_center));
        v2f d = v2i_to_v2f(sub2i(in_stroke->points[2], canvas_center));

        for ( i32 i = 3; i < num_points; ++i ) {
            a = b;
            b = c;
            c = d;
            d = v2i_to_v2f(sub2i(in_stroke->points[i], canvas_center));

            if ( out_i >= STROKE_MAX_POINTS-1 ) {
                break;  // Keep the stroke from becoming larger than we support.
            }

            float scale = 0.5f;
            v2f p0 = a;
            v2f p1 = sub2f(b, scale2f(sub2f(a, b), scale));
            v2f p2 = sub2f(c, scale2f(sub2f(d, c), scale));
            v2f p3 = d;

            // Diffs to calculate angle
            v2f d0 = sub2f(p0, p1);
            v2f d1 = sub2f(p2, p1);
            v2f d2 = sub2f(p1, p2);
            v2f d3 = sub2f(p3, p2);

            float n0 = fabs(DOT(d0, d1)) / (magnitude(d0) * magnitude(d1));
            float n1 = fabs(DOT(d2, d3)) / (magnitude(d2) * magnitude(d3));

            // If the sum of both angles is greater than this threshold, interpolate.
            float cos_min_angle = 0.05f;
            if ( 2 - n0 - n1 > cos_min_angle*2 ) {
                v2f n = {};
                n = add2f(n, scale2f(p0, 0.125));
                n = add2f(n, scale2f(p1, 0.375));
                n = add2f(n, scale2f(p2, 0.375));
                n = add2f(n, scale2f(p3, 0.125));

                out_stroke->pressures[out_i] = in_stroke->pressures[i - 2];
                out_stroke->points[out_i++] = add2i(v2f_to_v2i(b), canvas_center);

                out_stroke->pressures[out_i] = out_stroke->pressures[out_i-1]; // Use the same pressure value as last point.
                out_stroke->points[out_i++] = add2i(v2f_to_v2i(n), canvas_center);
            } else {
                out_stroke->points[out_i] = in_stroke->points[i-2];
                out_stroke->pressures[out_i++] = in_stroke->pressures[i-2];
            }

            // Always add the last point.
            if ( i == num_points - 1 ) {
                out_stroke->pressures[out_i] = in_stroke->pressures[i];
                out_stroke->points[out_i++] = in_stroke->points[i];
            }

        }

        out_stroke->num_points = out_i;
    }
    // Four or less points in stroke, or stroke is too large.
    else {
        out_stroke->points    = arena_alloc_array(arena, num_points, v2i);
        out_stroke->pressures = arena_alloc_array(arena, num_points, f32);

        memcpy(out_stroke->points, in_stroke->points, in_stroke->num_points * sizeof(v2i));
        memcpy(out_stroke->pressures, in_stroke->pressures, in_stroke->num_points * sizeof(f32));
        out_stroke->num_points = in_stroke->num_points;
    }
}

void
milton_update_and_render(MiltonState* milton_state, MiltonInput* input)
{
    PROFILE_GRAPH_BEGIN(update);

    b32 do_full_redraw = false;
    b32 brush_outline_should_draw = false;
    int render_flags = RenderDataFlags_NONE;

    b32 draw_custom_rectangle = false;  // Custom rectangle used for new strokes, undo/redo.
    Rect custom_rectangle = rect_without_size();

    b32 should_save =
            ((input->flags & MiltonInputFlags_OPEN_FILE)) ||
            ((input->flags & MiltonInputFlags_SAVE_FILE)) ||
            ((input->flags & MiltonInputFlags_END_STROKE)) ||
            ((input->flags & MiltonInputFlags_UNDO)) ||
            ((input->flags & MiltonInputFlags_REDO));

    if ( input->flags & MiltonInputFlags_OPEN_FILE ) {
        milton_load(milton_state);
        upload_gui(milton_state);
        input->flags |= MiltonInputFlags_FAST_DRAW;
        do_full_redraw = true;
    }

    if ( milton_state->flags & MiltonStateFlags_WORKER_NEEDS_MEMORY ) {
        milton_expand_render_memory(milton_state);
        do_full_redraw = true;
    }

    if ( milton_state->flags & MiltonStateFlags_REQUEST_QUALITY_REDRAW ) {
        milton_state->view->downsampling_factor = 1;  // See how long it takes to redraw at full quality
        milton_state->flags &= ~MiltonStateFlags_REQUEST_QUALITY_REDRAW;
        do_full_redraw = true;
    }

    i32 now = (i32)SDL_GetTicks();

    if ( (input->flags & MiltonInputFlags_FAST_DRAW) ) {
        milton_state->quality_redraw_time = now;
    }
    else if ( milton_state->quality_redraw_time > 0 &&
              (now - milton_state->quality_redraw_time) > QUALITY_REDRAW_TIMEOUT_MS ) {
        milton_state->flags |= MiltonStateFlags_REQUEST_QUALITY_REDRAW;  // Next update loop.
        milton_state->quality_redraw_time = 0;
    }

    if ( input->flags & MiltonInputFlags_FULL_REFRESH ) {
        do_full_redraw = true;
    }

    if ( input->scale ) {
        do_full_redraw = true;

// Debug
#if MILTON_ZOOM_DEBUG
        f32 scale_factor = 1.5f;
        i32 view_scale_limit = 1 << 20;
// Sensible
#else
        f32 scale_factor = 1.3f;
        i32 view_scale_limit = (1 << 13);
#endif

        // Some info on the reasoning behind choosing the values for different
        // knobs regarding scales.  We use the whole 32 bit address space for
        // integer coordinates, but there are tricky trade-offs between zoom
        // level and canvas length.  When fully zoomed-out, the canvas might be
        // too small in area available to pan. When fully zoomed-in, we run
        // into problems with the "arbitrary size" exporting. The latter
        // problem is trickier, but it is easy to see when imagining that at
        // full zoom-in, each screen pixel corresponds to a canvas unit. When
        // exporting at this scale, we can't stretch the canvas anymore! A
        // possible solution to this is to scale-up the strokes relative to the
        // center of the exporting rectangle. Another "solution" is to set a
        // maximum exporting scale coefficient, and set the minimum zoom to be
        // that as well.  The former solution is the more flexible. Taken to
        // its fullest potential it would allow a greater level of zoom-in. It
        // introduces more complexity, though.
        //
        // The current strategy is to enforece a min_scale
#if 0
        {
            int w = 2560;
            f32 ns = (f32)(1u << 31) / (w * view_scale_limit);
            f32 ms = (f32)(1u << 31) / (w * milton_state->view->scale);

            milton_log("With a screen width of %d, At this zoomout we would have %f horizontal screens. \n"
                       "At max zoomout we would have %f horizontal screens of movement.\n", w, ms, ns);

            i64 maxr = (1u << 31) - k_max_brush_size * view_scale_limit;
            milton_log ("max canvas radius is %ld\n", maxr);
        }
#endif

        i32 min_scale = MILTON_MINIMUM_SCALE;

        if ( input->scale > 0 && milton_state->view->scale >= min_scale ) {
            milton_state->view->scale = (i32)(ceilf(milton_state->view->scale / scale_factor));
        }
        else if ( input->scale < 0 && milton_state->view->scale < view_scale_limit ) {
            milton_state->view->scale = (i32)(milton_state->view->scale * scale_factor) + 1;
        }

        milton_update_brushes(milton_state);
        gpu_update_scale(milton_state->render_data, milton_state->view->scale);
    }
    else if ( (input->flags & MiltonInputFlags_PANNING) ) {
        // If we are *not* zooming and we are panning, we can copy most of the
        // framebuffer
        if ( !equ2i(input->pan_delta, v2i{}) ) {
            do_full_redraw = true;
        }
    }

    if ( (input->mode_to_set != MiltonMode_NONE) ) {
        MiltonMode mode = milton_state->current_mode;
        if ( mode == input->mode_to_set ) {
            // Modes we can toggle
            if ( mode == MiltonMode_EYEDROPPER ) {
                if ( milton_state->last_mode != MiltonMode_EYEDROPPER ) {
                    milton_use_previous_mode(milton_state);
                }
                else {
                    // This is not supposed to happen but if we get here we won't crash and burn.
                    milton_switch_mode(milton_state, MiltonMode_PEN);
                    milton_log("Warning: Unexpected code path: Toggling modes. Eye dropper was set *twice*. Switching to pen.");
                }
            }
        }
        // Change the current mode if it's different from the current mode.
        else {
            milton_switch_mode(milton_state, input->mode_to_set);
            if (    input->mode_to_set == MiltonMode_PEN
                 || input->mode_to_set == MiltonMode_ERASER ) {
                milton_update_brushes(milton_state);
            }
        }

    }

    { // Undo / Redo
        if ( (input->flags & MiltonInputFlags_UNDO) ) {
            // Grab undo elements. They might be from deleted layers, so discard dead results.
            while ( milton_state->canvas->history.count > 0 ) {
                HistoryElement h = pop(&milton_state->canvas->history);
                Layer* l = layer_get_by_id(milton_state->canvas->root_layer, h.layer_id);
                // found a thing to undo.
                if ( l ) {
                    if ( l->strokes.count > 0 ) {
                        Stroke* stroke_ptr = peek(&l->strokes);
                        Stroke stroke = pop(&l->strokes);
                        push(&milton_state->canvas->stroke_graveyard, stroke);
                        push(&milton_state->canvas->redo_stack, h);


                        draw_custom_rectangle = true;
                        Rect bounds = stroke.bounding_rect;
                        bounds.top_left = canvas_to_raster(milton_state->view, bounds.top_left);
                        bounds.bot_right = canvas_to_raster(milton_state->view, bounds.bot_right);

                        // Enlarge rectangle by one pixel to counter a floating point precision
                        // problem when canvas_to_raster_gl (in the shader) normalizes to [-1,1]^2
                        auto* view = milton_state->view;
                        bounds.top = max(0, bounds.top-1);
                        bounds.left = max(0, bounds.left-1);
                        bounds.right = min(view->screen_size.w, bounds.right+1);
                        bounds.bottom = min(view->screen_size.h, bounds.bottom+1);

                        custom_rectangle = rect_union(custom_rectangle, bounds);
                    }
                    break;
                }
            }
        }
        else if ( (input->flags & MiltonInputFlags_REDO) ) {
            if ( milton_state->canvas->redo_stack.count > 0 ) {
                HistoryElement h = pop(&milton_state->canvas->redo_stack);
                switch ( h.type ) {
                case HistoryElement_STROKE_ADD:
                    {
                    Layer* l = layer_get_by_id(milton_state->canvas->root_layer, h.layer_id);
                    if ( l && count(&milton_state->canvas->stroke_graveyard) > 0 ) {
                        Stroke stroke = pop(&milton_state->canvas->stroke_graveyard);
                        if ( stroke.layer_id == h.layer_id ) {
                            push(&l->strokes, stroke);
                            push(&milton_state->canvas->history, h);

                            draw_custom_rectangle = true;
                            Rect bounds = stroke.bounding_rect;
                            bounds.top_left = canvas_to_raster(milton_state->view, bounds.top_left);
                            bounds.bot_right = canvas_to_raster(milton_state->view, bounds.bot_right);
                            custom_rectangle = rect_union(custom_rectangle, bounds);

                            break;
                        }
                        stroke = pop(&milton_state->canvas->stroke_graveyard);  // Keep popping in case the graveyard has info from deleted layers
                    }

                    } break;
                /* case HistoryElement_LAYER_DELETE: { */
                /* } break; */
                }
            }
        }
    }

    // If the current mode is Pen or Eraser, we show the hover. It can be unset under various conditions later.
    if ( milton_state->current_mode == MiltonMode_PEN ||
         milton_state->current_mode == MiltonMode_ERASER ) {
        brush_outline_should_draw = true;
    }

    if ( (input->flags & MiltonInputFlags_HOVERING) ) {
        milton_state->hover_point = input->hover_point;
    }

    if ( gui_point_hovers(milton_state->gui, milton_state->hover_point) ) {
        brush_outline_should_draw = false;
    }

    // Set the hover point as the last point of the working stroke.
    if ( is_user_drawing(milton_state) ) {
        i32 np = milton_state->working_stroke.num_points;
        if ( np > 0 ) {
            milton_state->hover_point = canvas_to_raster(milton_state->view, milton_state->working_stroke.points[np-1]);
        }
    }

    if ( input->input_count > 0 || (input->flags | MiltonInputFlags_CLICK) ) {
        if ( current_mode_is_for_painting(milton_state) ) {
            if ( !is_user_drawing(milton_state)
                 && gui_consume_input(milton_state->gui, input) ) {
                milton_update_brushes(milton_state);
                gpu_update_picker(milton_state->render_data, &milton_state->gui->picker);
            }
            else if ( !milton_state->gui->owns_user_input
                      && (milton_state->canvas->working_layer->flags & LayerFlags_VISIBLE) ) {
                milton_stroke_input(milton_state, input);
            }
        }
    }

    if ( milton_state->current_mode == MiltonMode_EXPORTING ) {
        Exporter* exporter = &milton_state->gui->exporter;
        b32 changed = exporter_input(exporter, input);
        if ( changed ) {
            gpu_update_export_rect(milton_state->render_data, exporter);
        }
        if ( exporter->state != ExporterState_EMPTY ) {
             render_flags |= RenderDataFlags_EXPORTING;
        }
    }
    else if ( render_flags & RenderDataFlags_EXPORTING ) {
        render_flags &= ~RenderDataFlags_EXPORTING;
    }

    if ( (input->flags & MiltonInputFlags_IMGUI_GRABBED_INPUT) ) {
        // Start drawing the preview if we just grabbed a slider.
        brush_outline_should_draw = false;

        if ( (milton_state->gui->flags & MiltonGuiFlags_SHOWING_PREVIEW) ) {
            auto preview_pos = milton_state->gui->preview_pos;
            mlt_assert(preview_pos.x >= 0);
            mlt_assert(preview_pos.y >= 0);
            v4f color = {};
            color.rgb = milton_state->view->background_color;
            color.a = 1;
            if ( milton_state->current_mode == MiltonMode_PEN ) {
                color = to_premultiplied(hsv_to_rgb(milton_state->gui->picker.data.hsv),
                                         milton_get_pen_alpha(milton_state));
            }
            gpu_update_brush_outline(milton_state->render_data,
                                     preview_pos.x, preview_pos.y,
                                     milton_get_brush_radius(milton_state), BrushOutline_FILL, color);
        }
    } else {
        gui_imgui_set_ungrabbed(milton_state->gui);
    }

    if ( milton_state->gui->visible ) {
        render_flags |= RenderDataFlags_GUI_VISIBLE;
    } else {
        render_flags &= ~RenderDataFlags_GUI_VISIBLE;
    }

    if ( milton_state->current_mode == MiltonMode_EYEDROPPER ) {
        if ( milton_state->eyedropper_buffer == NULL ) {
            size_t bpp = 4;
            i32 w = milton_state->view->screen_size.w;
            i32 h = milton_state->view->screen_size.h;
            milton_state->eyedropper_buffer = (u8*)mlt_calloc(w*h*bpp, 1, "Bitmap");
            if ( milton_state->eyedropper_buffer ) {
                gpu_render_to_buffer(milton_state, milton_state->eyedropper_buffer, 1,
                                     0,0,w,h, 1.0f);
            }

        }
        v2i point = {0};
        b32 in = false;
        if ( (input->flags & MiltonInputFlags_CLICK) ) {
            point = input->click;
            in = true;
        }
        if ( (input->flags & MiltonInputFlags_HOVERING) ) {
            point = input->hover_point;
            in = true;
        }
        if ( in ) {
            mlt_assert(milton_state->eyedropper_buffer != NULL);
            eyedropper_input(milton_state->gui, (u32*)milton_state->eyedropper_buffer,
                             milton_state->view->screen_size.w,
                             milton_state->view->screen_size.h,
                             point);
            gpu_update_picker(milton_state->render_data, &milton_state->gui->picker);
        }
        if( input->flags & MiltonInputFlags_CLICKUP ) {
            if ( !(milton_state->flags & MiltonStateFlags_IGNORE_NEXT_CLICKUP) ) {
                milton_switch_mode(milton_state, MiltonMode_PEN);
                milton_update_brushes(milton_state);
            } else {
                milton_state->flags &= ~MiltonStateFlags_IGNORE_NEXT_CLICKUP;
            }
        }
    }

    // ---- End stroke
    if ( (input->flags & MiltonInputFlags_END_STROKE) ) {
        if ( milton_state->gui->owns_user_input ) {
            gui_deactivate(milton_state->gui);
            brush_outline_should_draw = false;
        } else {
            if ( milton_state->working_stroke.num_points > 0 ) {
                // We used the selected color to draw something. Push.
                if ( milton_state->current_mode == MiltonMode_PEN
                     && gui_mark_color_used(milton_state->gui) ) {
                    // Tell the renderer to update the picker
                    gpu_update_picker(milton_state->render_data, &milton_state->gui->picker);
                }
                // Copy current stroke.
                Stroke new_stroke = {};
                CanvasState* canvas = milton_state->canvas;
                copy_with_smooth_interpolation(&canvas->arena, milton_state->view, &milton_state->working_stroke, &new_stroke);
                {
                    new_stroke.brush = milton_state->working_stroke.brush;
                    new_stroke.layer_id = milton_state->view->working_layer_id;
                    new_stroke.bounding_rect = rect_union(bounding_box_for_stroke(&new_stroke),
                                                        bounding_box_for_stroke(&new_stroke));

                    new_stroke.id = milton_state->canvas->stroke_id_count++;

                    draw_custom_rectangle = true;
                    Rect bounds = new_stroke.bounding_rect;
                    bounds.top_left = canvas_to_raster(milton_state->view, bounds.top_left);
                    bounds.bot_right = canvas_to_raster(milton_state->view, bounds.bot_right);
                    custom_rectangle = rect_union(custom_rectangle, bounds);
                }

                mlt_assert(new_stroke.num_points > 0);
                mlt_assert(new_stroke.num_points <= STROKE_MAX_POINTS);
                auto* stroke = layer_push_stroke(milton_state->canvas->working_layer, new_stroke);

                // Invalidate working stroke render element

                HistoryElement h = { HistoryElement_STROKE_ADD, milton_state->canvas->working_layer->id };
                push(&milton_state->canvas->history, h);
                // Clear working_stroke
                {
                    milton_state->working_stroke.num_points = 0;
                    milton_state->working_stroke.render_element.count = 0;
                }

                clear_stroke_redo(milton_state);
            }
        }
    }
    else if ( is_user_drawing(milton_state) ) {
        milton_state->working_stroke.bounding_rect = bounding_box_for_stroke(&milton_state->working_stroke);
    }

    // Disable hover if panning.
    if ( input->flags & MiltonInputFlags_PANNING ) {
        brush_outline_should_draw = false;
    }

    #if MILTON_HARDWARE_BRUSH_CURSOR
        if ( milton_get_brush_radius(milton_state) < MILTON_HIDE_BRUSH_OVERLAY_AT_THIS_SIZE ) {
            brush_outline_should_draw = false;
        }
    #endif

    if ( !brush_outline_should_draw
         && (i32)SDL_GetTicks() - milton_state->hover_flash_ms < HOVER_FLASH_THRESHOLD_MS ) {
        brush_outline_should_draw = true;
    }


    if ( !(milton_state->gui->flags & MiltonGuiFlags_SHOWING_PREVIEW) ) {
        float radius = brush_outline_should_draw ? (float)milton_get_brush_radius(milton_state) : -1;
        gpu_update_brush_outline(milton_state->render_data,
                                milton_state->hover_point.x, milton_state->hover_point.y,
                                radius);
    }

    PROFILE_GRAPH_PUSH(update);

    if ( !(milton_state->flags & MiltonStateFlags_RUNNING) ) {
        platform_cursor_show();
    }

    if ( !(milton_state->flags & MiltonStateFlags_RUNNING) ) {
        // Someone tried to kill milton from outside the update. Make sure we save.
        should_save = true;
    }

    if ( should_save ) {
        if ( !(milton_state->flags & MiltonStateFlags_RUNNING) ) {
            milton_save(milton_state);
        } else {
#if MILTON_SAVE_ASYNC
            SDL_CreateThread(milton_save_async, "Async Save Thread", (void*)milton_state);
#else
            milton_save(milton_state);
#endif
        }
        // We're about to close and the last save failed and the drawing changed.
        if (    !(milton_state->flags & MiltonStateFlags_RUNNING)
             && (milton_state->flags & MiltonStateFlags_LAST_SAVE_FAILED)
             && (milton_state->flags & MiltonStateFlags_MOVE_FILE_FAILED)
             && milton_state->last_save_stroke_count != count_strokes(milton_state->canvas->root_layer) ) {
            // TODO: Stop using MoveFileEx?
            //  Why does MoveFileEx fail? Ask someone who knows this stuff.
            // Wait a moment and try again. If this fails, prompt to save somewhere else.
            SDL_Delay(3000);
            milton_save(milton_state);

            if (    (milton_state->flags & MiltonStateFlags_LAST_SAVE_FAILED)
                 && (milton_state->flags & MiltonStateFlags_MOVE_FILE_FAILED) ) {
                char msg[1024];
                WallTime lst = milton_state->last_save_time;
                snprintf(msg, 1024, "Milton failed to save this canvas. The last successful save was at %.2d:%.2d:%.2d. Try saving to another file?",
                         lst.hours, lst.minutes, lst.seconds);
                b32 another = platform_dialog_yesno(msg, "Try another file?");
                if ( another ) {
                    // NOTE(possible refactor): There is similar code. Guipp.cpp save_milton_canvas
                    PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                    if ( name ) {
                        milton_log("Saving to %s\n", name);
                        milton_set_canvas_file(milton_state, name);
                        milton_save(milton_state);
                        if ( (milton_state->flags & MiltonStateFlags_LAST_SAVE_FAILED) ) {
                            platform_dialog("Still can't save. Please contact us for help. miltonpaint.com", "Info");
                        } else {
                            platform_dialog("Success.", "Info");
                        }
                        b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"), DeleteErrorTolerance_OK_NOT_EXIST);
                        if ( del == false ) {
                            platform_dialog("Could not delete default canvas."
                                            " Contents will be still there when you create a new canvas.", "Info");
                        }
                    }
                }
            }
        }

        // About to quit and just saved.
        if ( !(milton_state->flags & MiltonStateFlags_RUNNING) ) {
            // Release resources
            milton_reset_canvas(milton_state);
            gpu_release_data(milton_state->render_data);

            #if DEBUG_MEMORY_USAGE
                debug_memory_dump_allocations();
            #endif
        }
    }

    profiler_output();

    i32 view_x = 0;
    i32 view_y = 0;
    i32 view_width = 0;
    i32 view_height = 0;

    gpu_reset_render_flags(milton_state->render_data, render_flags);

    ClipFlags clip_flags = ClipFlags_JUST_CLIP;

#if REDRAW_EVERY_FRAME
    do_full_redraw = true;
#endif
    // Note: We flip the rectangles. GL is bottom-left by default.
    if ( do_full_redraw ) {
        view_width = milton_state->view->screen_size.w;
        view_height = milton_state->view->screen_size.h;
        // Only update GPU data if we are redrawing the full screen. This means
        // that the size of the screen will be used to determine if each stroke
        // should be freed from GPU memory.
        clip_flags = ClipFlags_UPDATE_GPU_DATA;
    }
    else if ( draw_custom_rectangle ) {
        view_x = custom_rectangle.left;
        view_y = custom_rectangle.top;
        //view_y = milton_state->view->screen_size.h - custom_rectangle.bottom;
        view_width = custom_rectangle.right - custom_rectangle.left;
        view_height = custom_rectangle.bottom - custom_rectangle.top;
        VALIDATE_RECT(custom_rectangle);
    }
    else if ( milton_state->working_stroke.num_points > 0 ) {
        Rect bounds = milton_state->working_stroke.bounding_rect;
        bounds.top_left = canvas_to_raster(milton_state->view, bounds.top_left);
        bounds.bot_right = canvas_to_raster(milton_state->view, bounds.bot_right);
        view_x = bounds.left;
        view_y = bounds.top;
        // view_y = milton_state->view->screen_size.h - bounds.bottom;

        view_width = bounds.right - bounds.left;
        view_height = bounds.bottom - bounds.top;
    }

    PROFILE_GRAPH_BEGIN(clipping);
    gpu_clip_strokes_and_update(&milton_state->root_arena, milton_state->render_data, milton_state->view,
                                milton_state->canvas->root_layer, &milton_state->working_stroke,
                                view_x, view_y, view_width, view_height, clip_flags);
    PROFILE_GRAPH_PUSH(clipping);

    gpu_render(milton_state->render_data, view_x, view_y, view_width, view_height);

    //milton_validate(milton_state);
    ARENA_VALIDATE(&milton_state->root_arena);
}

