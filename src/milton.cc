// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "milton.h"

#include "common.h"
#include "color.h"
#include "canvas.h"
#include "gui.h"
#include "renderer.h"
#include "localization.h"
#include "persist.h"
#include "platform.h"
#include "vector.h"
#include "bindings.h"

// Defined below.
static void milton_validate(Milton* milton);

void
milton_set_background_color(Milton* milton, v3f background_color)
{
    milton->view->background_color = background_color;
    gpu_update_background(milton->render_data, milton->view->background_color);
}

static void
milton_set_default_view(Milton* milton)
{
    milton_log("Setting default view\n");
    CanvasView* view = milton->view;

    auto size = view->screen_size;

    *view = CanvasView{};

    view->background_color = milton->settings->background_color;
    view->screen_size      = size;
    view->zoom_center      = size / 2;
    view->scale            = MILTON_DEFAULT_SCALE;
    view->num_layers       = 1;
}

int
milton_get_brush_enum(Milton* milton)
{
    int brush_enum = BrushEnum_NOBRUSH;
    switch ( milton->current_mode ) {
        case MiltonMode::PEN: {
            brush_enum = BrushEnum_PEN;
        } break;
        case MiltonMode::ERASER: {
            brush_enum = BrushEnum_ERASER;
        } break;
        case MiltonMode::PRIMITIVE: {
            brush_enum = BrushEnum_PRIMITIVE;
        } break;
        case MiltonMode::EXPORTING:
        case MiltonMode::EYEDROPPER:
        case MiltonMode::HISTORY:
        case MiltonMode::NONE:
        default: {
        } break;
    }
    return brush_enum;
}

static void
milton_update_brushes(Milton* milton)
{
    for ( int i = 0; i < BrushEnum_COUNT; ++i ) {
        Brush* brush = &milton->brushes[i];
        i32 size = milton->brush_sizes[i];
        brush->radius = size * milton->view->scale;
        mlt_assert(brush->radius < FLT_MAX);
        if ( i == BrushEnum_PEN ) {
            // Alpha is set by the UI
            brush->color = to_premultiplied(gui_get_picker_rgb(milton->gui), brush->alpha);

            // Check for eraser magic value (see blend.f.glsl and layer_blend.f.glsl)
            if ( brush->color.r == 0.0f &&
                 brush->color.g == 1.0f &&
                 brush->color.b == 0.0f &&
                 brush->color.a == 1.0f ) {
                // Grab the largest float that's less than 1.0f
                *reinterpret_cast<u32*>(&brush->color.g) -= 1;
            }
        }
        else if ( i == BrushEnum_ERASER ) {
            brush->color = k_eraser_color;
        }
        else if ( i == BrushEnum_PRIMITIVE ) {
            brush->color = to_premultiplied(gui_get_picker_rgb(milton->gui), brush->alpha);
        }
    }

    int brush_enum = milton_get_brush_enum(milton);
    milton->working_stroke.brush = milton->brushes[brush_enum];
}


// Eyedropper
void
eyedropper_init(Milton* milton)
{
    size_t bpp = 4;
    i32 w = milton->view->screen_size.w;
    i32 h = milton->view->screen_size.h;

    Eyedropper* e = milton->eyedropper;
    if (!e->buffer)
    {
        e->buffer = (u8*)mlt_calloc(w*h*bpp, 1, "Bitmap");
        gpu_render_to_buffer(milton, e->buffer, 1, 0, 0, w, h, 1.0f);
    }
}

void
eyedropper_input(Eyedropper* e, MiltonGui* gui, i32 w, i32 h, v2i point)
{

    u32* pixels = (u32*)e->buffer;
    if ( point.y > 0 && point.y <= h && point.x > 0 && point.x <= w ) {
        v4f color = color_u32_to_v4f(pixels[point.y * w + point.x]);
        gui_picker_from_rgb(&gui->picker, color.rgb);
    }
}

void
eyedropper_deinit(Eyedropper* e)
{
    if ( e->buffer ) {
        mlt_free(e->buffer, "Bitmap");
        e->buffer = NULL;
    }
}

static Brush
milton_get_brush(Milton* milton)
{
    int brush_enum = milton_get_brush_enum(milton);

    Brush brush = milton->brushes[brush_enum];

    return brush;
}

static i32*
pointer_to_brush_size(Milton* milton)
{
    int brush_enum = milton_get_brush_enum(milton);
    i32* ptr = &milton->brush_sizes[brush_enum];
    return ptr;
}

static b32
is_user_drawing(Milton* milton)
{
    b32 result = milton->working_stroke.num_points > 0;
    return result;
}

static b32
mode_is_for_drawing(MiltonMode mode)
{
    b32 result = mode == MiltonMode::PEN ||
            mode == MiltonMode::ERASER ||
            mode == MiltonMode::PRIMITIVE;
    return result;
}

b32
current_mode_is_for_drawing(Milton* milton)
{
    return mode_is_for_drawing(milton->current_mode);
}

static void
clear_stroke_redo(Milton* milton)
{
    while ( milton->canvas->stroke_graveyard.count > 0 ) {
        Stroke s = pop(&milton->canvas->stroke_graveyard);
    }
    for ( i64 i = 0; i < milton->canvas->redo_stack.count; ++i ) {
        HistoryElement h = milton->canvas->redo_stack.data[i];
        if ( h.type == HistoryElement_STROKE_ADD ) {
            for ( i64 j = i; j < milton->canvas->redo_stack.count-1; ++j ) {
                milton->canvas->redo_stack.data[j] = milton->canvas->redo_stack.data[j+1];
            }
            pop(&milton->canvas->redo_stack);
        }
    }
}

static void
milton_primitive_input(Milton* milton, MiltonInput* input, b32 end_stroke)
{
    if ( end_stroke && milton->primitive_fsm == Primitive_DRAWING) {
       milton->primitive_fsm = Primitive_WAITING;
    }
    else if (input->input_count > 0) {
        v2l point = raster_to_canvas(milton->view, input->points[input->input_count - 1]);
        Stroke* ws = &milton->working_stroke;
        if ( milton->primitive_fsm == Primitive_WAITING ) {
            milton->primitive_fsm             = Primitive_DRAWING;
            ws->points[0]  = ws->points[1] = point;
            ws->pressures[0] = ws->pressures[1] = 1.0f;
            milton->working_stroke.num_points = 2;
            ws->brush                         = milton_get_brush(milton);
            ws->layer_id                      = milton->view->working_layer_id;
        }
        else if ( milton->primitive_fsm == Primitive_DRAWING ) {
           milton->working_stroke.points[1] = point;
        }
    }
}

void
stroke_append_point(Stroke* stroke, v2l canvas_point, f32 pressure)
{
    b32 not_the_first =  stroke->num_points >= 1;

    // A point passes inspection if:
    // Add to current stroke.
    if ( stroke->num_points < STROKE_MAX_POINTS ) {
        int index = stroke->num_points++;
        stroke->points[index] = canvas_point;
        stroke->pressures[index] = pressure;
    }
}

static v2l
smooth_filter(SmoothFilter* filter, v2l input)
{
    filter->points[filter->index++] = input;
    filter->index %= SMOOTHING_WINDOW;

    float factor = 1.0 / SMOOTHING_WINDOW;

    v2f sum = {0};
    for (int i = 0; i < SMOOTHING_WINDOW; ++i) {
        v2f point = {
            (float)filter->points[i].x - input.x,
            (float)filter->points[i].y - input.y,
        };

        sum = sum + point * factor;
    }

    v2l result = {
        (long)sum.x + input.x,
        (long)sum.y + input.y,
    };

    return result;
}

static void
clear_smooth_filter(SmoothFilter* filter, v2l value)
{
    for (int i = 0; i < SMOOTHING_WINDOW; ++i) {
        filter->points[i] = value;
    }
}

static u64 peek_out_duration_ms(Milton* milton);  // forward decl


i64
milton_render_scale(Milton* milton)
{
    if ( milton->current_mode == MiltonMode::PEEK_OUT ) {
        WallTime time = platform_get_walltime();

        u64 ms = difference_in_ms(milton->peek_out->begin_anim_time, time);

        float interp = 0.0f;
        if (ms < peek_out_duration_ms(milton)) {
            interp = ms / (float)peek_out_duration_ms(milton);
        }
        else {
            interp = 1.0f;
        }

        if (milton->peek_out->peek_out_ended) {
            interp = 1 - interp;
        }

        return lerp(milton->peek_out->low_scale, milton->peek_out->high_scale, interp);
    }
    else {
        return milton->view->scale;
    }
}

static void
milton_stroke_input(Milton* milton, MiltonInput* input)
{
    if ( input->input_count == 0 ) {
        return;
    }

    if ( milton->view->scale != milton_render_scale(milton) ) {
        return;  // We can't draw while peeking.
    }

    // Using the pan center to do a change of coordinates so that we
    // don't lose precision when we convert to floating point
    v2l pan_center = milton->view->pan_center;

    Stroke* ws = &milton->working_stroke;

    if ((milton->flags & MiltonStateFlags_BRUSH_SMOOTHING) && ws->num_points == 0) {
        clear_smooth_filter(milton->smooth_filter, input->points[0]);
    }

    //milton_log("Stroke input with %d packets\n", input->input_count);
    ws->brush    = milton_get_brush(milton);
    ws->layer_id = milton->view->working_layer_id;

    for ( int input_i = 0; input_i < input->input_count; ++input_i ) {

        v2l in_point = input->points[input_i];
        if (milton->flags & MiltonStateFlags_BRUSH_SMOOTHING) {
            in_point = smooth_filter(milton->smooth_filter, in_point);
        }

        v2l canvas_point = raster_to_canvas(milton->view, in_point);

        f32 pressure = NO_PRESSURE_INFO;

        if ( input->pressures[input_i] != NO_PRESSURE_INFO ) {
            f32 pressure_min = 0.01f;
            pressure = pressure_min + input->pressures[input_i] * (1.0f - pressure_min);
        } else {
            pressure = 1.0f;
        }

        stroke_append_point(ws, canvas_point, pressure);
    }
}

void
milton_set_zoom_at_point(Milton* milton, v2i new_zoom_center)
{
    milton->view->pan_center += VEC2L(new_zoom_center - milton->view->zoom_center) * milton_render_scale(milton);

    milton->view->zoom_center = new_zoom_center;
    gpu_update_canvas(milton->render_data, milton->canvas, milton->view);
}

void
milton_set_zoom_at_screen_center(Milton* milton)
{
    milton_set_zoom_at_point(milton, milton->view->screen_size / 2);
}

void
milton_set_canvas_file_(Milton* milton, PATH_CHAR* fname, b32 is_default)
{
    milton_log("Set milton file: %s\n", fname);
    if ( is_default ) {
        milton->flags |= MiltonStateFlags_DEFAULT_CANVAS;
    } else {
        milton->flags &= ~MiltonStateFlags_DEFAULT_CANVAS;
    }

    u64 len = PATH_STRLEN(fname);
    if ( len > MAX_PATH ) {
        milton_log("milton_set_canvas_file: fname was too long %lu\n", len);
        fname = TO_PATH_STR("MiltonPersist.mlt");
    }
    milton->persist->mlt_file_path = fname;

    if ( !is_default ) {
        milton_set_last_canvas_fname(fname);
    } else {
        milton_unset_last_canvas_fname();
    }
}

void
milton_set_canvas_file(Milton* milton, PATH_CHAR* fname)
{
    milton_set_canvas_file_(milton, fname, false);
}

// Helper function
void
milton_set_default_canvas_file(Milton* milton)
{
    PATH_CHAR* f = (PATH_CHAR*)mlt_calloc(MAX_PATH, sizeof(PATH_CHAR), "Strings");

    PATH_STRNCPY(f, TO_PATH_STR("MiltonPersist.mlt"), MAX_PATH);
    platform_fname_at_config(f, MAX_PATH);
    milton_set_canvas_file_(milton, f, true);
    milton->flags |= MiltonStateFlags_DEFAULT_CANVAS;
}

i32
milton_get_brush_radius(Milton* milton)
{
    int brush_enum = milton_get_brush_enum(milton);
    i32 brush_size = milton->brush_sizes[brush_enum];
    if ( brush_size <= 0 ) {
       brush_size = 1;
    }
    return brush_size;
}

void
milton_set_brush_size(Milton* milton, i32 size)
{
    if ( current_mode_is_for_drawing(milton) ) {
        if ( size <= MILTON_MAX_BRUSH_SIZE && size > 0 ) {
            (*pointer_to_brush_size(milton)) = size;
            milton_update_brushes(milton);
        }
    }
}

// For keyboard shortcut.
void
milton_increase_brush_size(Milton* milton)
{
    if ( current_mode_is_for_drawing(milton) ) {
        i32 brush_size = milton_get_brush_radius(milton);
        if ( brush_size < MILTON_MAX_BRUSH_SIZE && brush_size > 0 ) {
            milton_set_brush_size(milton, brush_size + 1);
        }
        milton_update_brushes(milton);
    }
}

// For keyboard shortcut.
void
milton_decrease_brush_size(Milton* milton)
{
    if ( current_mode_is_for_drawing(milton) ) {
        i32 brush_size = milton_get_brush_radius(milton);
        if ( brush_size > 1 ) {
            milton_set_brush_size(milton, brush_size - 1);
        }
        milton_update_brushes(milton);
    }
}

void
milton_set_brush_alpha(Milton* milton, float alpha)
{
    int brush_enum = milton_get_brush_enum(milton);

    milton->brushes[brush_enum].alpha = alpha;
    milton_update_brushes(milton);
}

float
milton_get_brush_alpha(Milton* milton)
{
    int brush_enum = milton_get_brush_enum(milton);
    const float alpha = milton->brushes[brush_enum].alpha;
    return alpha;
}

void
settings_init(MiltonSettings* s)
{
    s->background_color = v3f{1,1,1};
    s->peek_out_increment = DEFAULT_PEEK_OUT_INCREMENT_LOG;
}

int milton_save_thread(void* state_);  // forward

void
reset_working_stroke(Milton* milton)
{
    milton->working_stroke.num_points = 0;
    milton->working_stroke.render_element.count = 0;
    milton->working_stroke.bounding_rect = rect_without_size();
}


void
milton_init(Milton* milton, i32 width, i32 height, f32 ui_scale, PATH_CHAR* file_to_open, MiltonInitFlags init_flags)
{
    b32 init_graphics = !(init_flags & MiltonInit_FOR_TEST);
    b32 read_from_disk = !(init_flags & MiltonInit_FOR_TEST);

    init_localization();

    milton->canvas = arena_bootstrap(CanvasState, arena, 1024*1024);
    milton->working_stroke.points    = arena_alloc_array(&milton->root_arena, STROKE_MAX_POINTS, v2l);
    milton->working_stroke.pressures = arena_alloc_array(&milton->root_arena, STROKE_MAX_POINTS, f32);
#if STROKE_DEBUG_VIZ
    milton->working_stroke.debug_flags = arena_alloc_array(&milton->root_arena, STROKE_MAX_POINTS, int);
#endif

    reset_working_stroke(milton);

    milton->current_mode = MiltonMode::PEN;
    milton->last_mode = MiltonMode::PEN;

    milton->render_data = gpu_allocate_render_data(&milton->root_arena);

    milton->smooth_filter = arena_alloc_elem(&milton->root_arena, SmoothFilter);

    if (init_graphics) { milton->gl = arena_alloc_elem(&milton->root_arena, MiltonGLState); }
    milton->gui = arena_alloc_elem(&milton->root_arena, MiltonGui);
    milton->settings = arena_alloc_elem(&milton->root_arena, MiltonSettings);
    milton->eyedropper = arena_alloc_elem(&milton->root_arena, Eyedropper);
    milton->persist = arena_alloc_elem(&milton->root_arena, MiltonPersist);

    milton->persist->target_MB_per_sec = 0.2f;

    gui_init(&milton->root_arena, milton->gui, ui_scale);
    settings_init(milton->settings);

    milton->peek_out = arena_alloc_elem(&milton->root_arena, PeekOut);

    b32 loaded_settings = false;
    if (read_from_disk) {
        loaded_settings = milton_settings_load(milton->settings);
    }

    if (!loaded_settings) {
        set_default_bindings(&milton->settings->bindings);
    }

    milton->view = arena_alloc_elem(&milton->root_arena, CanvasView);
    milton_set_default_view(milton);

    milton->view->screen_size = { width, height };

    if (init_graphics) { gpu_init(milton->render_data, milton->view, &milton->gui->picker); }

    if (init_graphics) { gpu_update_background(milton->render_data, milton->view->background_color); }

    { // Get/Set Milton Canvas (.mlt) file
        if ( file_to_open == NULL ) {
            PATH_CHAR* last_fname = milton_get_last_canvas_fname();

            if ( last_fname != NULL ) {
                milton_set_canvas_file(milton, last_fname);
            } else {
                milton_set_default_canvas_file(milton);
            }
        }
        else {
            milton_set_canvas_file(milton, file_to_open);
        }
    }

    // Set default brush sizes.
    for ( int i = 0; i < BrushEnum_COUNT; ++i ) {
        switch ( i ) {
        case BrushEnum_PEN: {
           milton->brush_sizes[i] = 10;
        } break;
        case BrushEnum_ERASER: {
           milton->brush_sizes[i] = 40;
        } break;
        case BrushEnum_PRIMITIVE: {
           milton->brush_sizes[i] = 10;
        } break;
        case BrushEnum_NOBRUSH: { {
           milton->brush_sizes[i] = 1;
        } } break;
        default: {
           INVALID_CODE_PATH;
        } break;
        }
        mlt_assert(milton->brush_sizes[i] > 0 && milton->brush_sizes[i] <= MILTON_MAX_BRUSH_SIZE);
    }
    milton_set_brush_alpha(milton, 1.0f);

    milton->persist->last_save_time = {};
    // Note: This will fill out uninitialized data like default layers.
    if (read_from_disk) { milton_load(milton); }

    milton_validate(milton);

    // Enable brush smoothing by default
    if ( !milton_brush_smoothing_enabled(milton) ) {
        milton_toggle_brush_smoothing(milton);
    }

#if MILTON_ENABLE_PROFILING
    milton->viz_window_visible = false;  // hidden by default
#endif

    milton->flags |= MiltonStateFlags_RUNNING;

#if MILTON_ENABLE_PROFILING
    profiler_init();
#endif

    #if MILTON_SAVE_ASYNC
        milton->save_mutex = SDL_CreateMutex();
        milton->save_cond = SDL_CreateCond();
        milton->save_thread = SDL_CreateThread(milton_save_thread, "Save thread", (void*)milton);
    #endif
}

void
upload_gui(Milton* milton)
{
    if (milton->gl)
    {
        gpu_update_canvas(milton->render_data, milton->canvas, milton->view);
        gpu_resize(milton->render_data, milton->view);
        gpu_update_picker(milton->render_data, &milton->gui->picker);
    }
}

// Returns false if the pan_delta moves the pan vector outside of the canvas.
void
milton_resize_and_pan(Milton* milton, v2l pan_delta, v2i new_screen_size)
{
    if ( milton->max_width <= new_screen_size.w ) {
        milton->max_width = new_screen_size.w + 256;
    }
    if ( milton->max_height <= new_screen_size.h ) {
        milton->max_height = new_screen_size.h + 256;
    }

    if ( new_screen_size.w < milton->max_width && new_screen_size.h < milton->max_height ) {
        milton->view->screen_size = new_screen_size;

        // Add delta to pan vector
        v2l pan_center = milton->view->pan_center - (pan_delta * milton->view->scale);

        milton->view->pan_center = pan_center;

        upload_gui(milton);
    } else {
        milton_die_gracefully("Fatal error. Screen size is more than Milton can handle.");
    }
}

void
milton_reset_canvas(Milton* milton)
{
    CanvasState* canvas = milton->canvas;

    gpu_free_strokes(milton->render_data, milton->canvas);
    milton->persist->mlt_binary_version = MILTON_MINOR_VERSION;
    milton->persist->last_save_time = {};

    // Clear history
    release(&canvas->history);
    release(&canvas->redo_stack);
    release(&canvas->stroke_graveyard);

    size_t size = canvas->arena.min_block_size;
    arena_free(&canvas->arena);  // Note: This destroys the canvas
    milton->canvas = arena_bootstrap(CanvasState, arena, size);

    mlt_assert(milton->canvas->history.count == 0);
}

void
milton_reset_canvas_and_set_default(Milton* milton)
{
    milton_reset_canvas(milton);

    // New Root
    milton_new_layer(milton);

    // New View
    milton_set_default_view(milton);
    milton->view->background_color = milton->settings->background_color;
    gpu_update_background(milton->render_data, milton->view->background_color);

    // Reset color buttons
    for ( ColorButton* b = milton->gui->picker.color_buttons; b!=NULL; b=b->next ) {
        b->rgba = {};
    }

    // gui init
    {
        MiltonGui* gui = milton->gui;
        gui->picker.data.hsv = { 0.0f, 1.0f, 0.7f };
        gui->visible = true;

        picker_init(&gui->picker);

        gui->preview_pos      = v2i{-1, -1};
        gui->preview_pos_prev = v2i{-1, -1};

        exporter_init(&gui->exporter);
    }

    milton_update_brushes(milton);

    milton_set_default_canvas_file(milton);
    upload_gui(milton);
}

void
milton_switch_mode(Milton* milton, MiltonMode mode)
{
    if ( mode != milton->current_mode ) {
        milton->last_mode = milton->current_mode;
        milton->current_mode = mode;

        if ( milton->last_mode == MiltonMode::EYEDROPPER) {
            eyedropper_deinit(milton->eyedropper);
        }

        if ( mode == MiltonMode::EXPORTING && milton->gui->visible ) {
            gui_toggle_visibility(milton->gui);
        }
        if ( milton->last_mode == MiltonMode::EXPORTING && !milton->gui->visible ) {
            gui_toggle_visibility(milton->gui);
        }
    }
}

void
milton_use_previous_mode(Milton* milton)
{
    milton_switch_mode(milton, milton->last_mode);
}

void
milton_try_quit(Milton* milton)
{
    milton->flags &= ~MiltonStateFlags_RUNNING;
}

void
milton_save_postlude(Milton* milton)
{
    MiltonPersist* p = milton->persist;
    p->last_save_time = platform_get_walltime();
    p->last_save_stroke_count = layer::count_strokes(milton->canvas->root_layer);

    milton->flags &= ~MiltonStateFlags_LAST_SAVE_FAILED;
}

#if MILTON_SAVE_ASYNC
void
trigger_async_save(Milton* milton)
{
    SDL_LockMutex(milton->save_mutex);
    {
        milton->save_flag = SaveEnum_SAVE_REQUESTED;
    }
    SDL_UnlockMutex(milton->save_mutex);
}

void
milton_kill_save_thread(Milton* milton)
{
    SDL_LockMutex(milton->save_mutex);
    milton->save_flag = SaveEnum_KILL;
    SDL_UnlockMutex(milton->save_mutex);

    // Do a save tick.
    SDL_LockMutex(milton->save_mutex);
    SDL_CondSignal(milton->save_cond);
    SDL_UnlockMutex(milton->save_mutex);

    SDL_WaitThread(milton->save_thread, NULL);
}

int
milton_save_thread(void* state_)
{
    Milton* milton = (Milton*)state_;
    MiltonPersist* p = milton->persist;

    b32 running = true;
    float time_to_wait_s = 0.0f;
    u64 wait_begin_us = perf_counter();

    while ( running ) {
        bool do_save = false;
        SDL_LockMutex(milton->save_mutex);

        SDL_CondWait(milton->save_cond, milton->save_mutex); // Wait for a frame tick.

        if ( milton->save_flag == SaveEnum_KILL ) {
            running = false;
        }
        else {
            float time_waited_s = perf_count_to_sec(perf_counter() - wait_begin_us);
            if (time_waited_s <= time_to_wait_s) {
                time_to_wait_s -= time_waited_s;
            }
            else {
                if ( milton->save_flag == SaveEnum_SAVE_REQUESTED ) {
                    do_save = true;
                    milton->save_flag = SaveEnum_WAITING;
                }
            }
			wait_begin_us = perf_counter();
        }
        SDL_UnlockMutex(milton->save_mutex);

        if ( do_save ) {
            // Wait. Either one frame, or the time to stay below bandwidth.
            u64 begin_us = perf_counter();
            u64 bytes_written = milton_save(milton);
            u64 duration_us = perf_counter() - begin_us;

            // Sleep, if necessary.
            float duration_s = duration_us / 1000000.0f;

            float MB_written = bytes_written / (1024.0f * 1024.0f);
            float MB_per_sec = MB_written / duration_s;

            if (MB_per_sec > p->target_MB_per_sec) {
                time_to_wait_s = MB_written / p->target_MB_per_sec - duration_s;
                wait_begin_us = perf_counter();
            }
        }
    }
    return 0;
}
#endif

void
milton_new_layer(Milton* milton)
{
    CanvasState* canvas = milton->canvas;
    i32 id = canvas->layer_guid++;
    milton_log("Increased guid to %d\n", canvas->layer_guid);
    milton_new_layer_with_id(milton, id);
}

void
milton_new_layer_with_id(Milton* milton, i32 new_id)
{
    CanvasState* canvas = milton->canvas;
    Layer* layer = arena_alloc_elem(&canvas->arena, Layer);
    {
        layer->id = new_id;
        layer->flags = LayerFlags_VISIBLE;
        layer->strokes.arena = &canvas->arena;
        layer->alpha = 1.0f;
        strokelist_init_bucket(&layer->strokes.root);
    }
    snprintf(layer->name, MAX_LAYER_NAME_LEN, "Layer %d", layer->id);

    if ( canvas->root_layer != NULL ) {
        Layer* top = layer::get_topmost(canvas->root_layer);
        top->next = layer;
        layer->prev = top;
        milton_set_working_layer(milton, top->next);
    } else {
        canvas->root_layer = layer;
        milton_set_working_layer(milton, layer);
    }
}

void
milton_set_working_layer(Milton* milton, Layer* layer)
{
    milton->canvas->working_layer = layer;
    milton->view->working_layer_id = layer->id;
}

void milton_wipe_layer(Milton* milton)
{
    layer::layer_wipe(milton->canvas->working_layer);
}

void
milton_delete_working_layer(Milton* milton)
{
    Layer* layer = milton->canvas->working_layer;
    if ( layer->next || layer->prev ) {
        if (layer->next) layer->next->prev = layer->prev;
        if (layer->prev) layer->prev->next = layer->next;

        Layer* wl = NULL;
        if (layer->next) wl = layer->next;
        else wl = layer->prev;
        milton_set_working_layer(milton, wl);
    }
    if ( layer == milton->canvas->root_layer ) {
        milton->canvas->root_layer = milton->canvas->working_layer;
    }
}

b32
milton_brush_smoothing_enabled(Milton* milton)
{
    b32 enabled = (milton->flags & MiltonStateFlags_BRUSH_SMOOTHING);
    return enabled;
}

void
milton_toggle_brush_smoothing(Milton* milton)
{
    if ( milton_brush_smoothing_enabled(milton) ) {
        milton->flags &= ~MiltonStateFlags_BRUSH_SMOOTHING;
    } else {
        milton->flags |= MiltonStateFlags_BRUSH_SMOOTHING;
    }
}

static void
milton_validate(Milton* milton)
{
    // Make sure that the history reflects the strokes that exist
    i64 num_layers=0;
    for ( Layer* l = milton->canvas->root_layer; l != NULL; l = l->next ) {
        ++num_layers;
    }
    i32* layer_ids = (i32*)mlt_calloc((size_t)num_layers, sizeof(i32), "Validate");
    i64 i = 0;
    for ( Layer* l = milton->canvas->root_layer; l != NULL; l = l->next ) {
        layer_ids[i] = l->id;
        ++i;
    }

    i64 history_count = 0;
    for ( i64 hi = 0; hi < milton->canvas->history.count; ++hi ) {
        i32 id = milton->canvas->history.data[hi].layer_id;
        for ( i64 li = 0; li < num_layers; ++li ) {
            if ( id == layer_ids[li] ) {
                ++history_count;
            }
        }
    }

    i64 stroke_count = layer::count_strokes(milton->canvas->root_layer);
    if ( history_count != stroke_count ) {
        milton_log("WARNING: Recreating history. File says History: %d(max %d) Actual strokes: %d\n",
                   history_count, milton->canvas->history.count,
                   stroke_count);
        reset(&milton->canvas->history);
        i32 id = 0;
        for ( Layer *l = milton->canvas->root_layer;
              l != NULL;
              l = l->next ) {
            for ( i64 si = 0; si < l->strokes.count; ++si ) {
                Stroke* s = get(&l->strokes, si);
                HistoryElement he = { HistoryElement_STROKE_ADD, s->layer_id };
                push(&milton->canvas->history, he);
            }
        }
    }

    mlt_free(layer_ids, "Validate");
}


// Copy points from in_stroke to out_stroke, but do interpolation to smooth it out.
static void
copy_stroke(Arena* arena, CanvasView* view, Stroke* in_stroke, Stroke* out_stroke)
{
    i32 num_points = in_stroke->num_points;
    // Shallow copy
    *out_stroke = *in_stroke;

    // Deep copy
    out_stroke->points    = arena_alloc_array(arena, num_points, v2l);
    out_stroke->pressures = arena_alloc_array(arena, num_points, f32);

    memcpy(out_stroke->points, in_stroke->points, num_points * sizeof(v2l));
    memcpy(out_stroke->pressures, in_stroke->pressures, num_points * sizeof(f32));

#if STROKE_DEBUG_VIZ
    out_stroke->debug_flags = arena_alloc_array(arena, num_points * sizeof(int), int);
    memcpy(out_stroke->debug_flags, in_stroke->debug_flags, num_points*sizeof(int));
#endif

    out_stroke->render_element = {};
}

static i64
peek_out_target_scale(Milton* milton)
{
    double log_scale = log2(1 + milton->view->scale) / log2(SCALE_FACTOR);
    i64 target = min(pow(SCALE_FACTOR, log_scale + milton->settings->peek_out_increment), VIEW_SCALE_LIMIT);
    return target;
}

static u64
peek_out_duration_ms(Milton* milton)
{
    u64 duration = milton->settings->peek_out_increment * PEEK_OUT_SPEED;
    return duration;
}

void
peek_out_trigger_start(Milton* milton)
{
    milton_set_zoom_at_screen_center(milton);

    milton->peek_out->low_scale = milton_render_scale(milton);
    milton->peek_out->high_scale = peek_out_target_scale(milton);
    milton->peek_out->peek_out_ended = false;
    milton->peek_out->begin_anim_time = platform_get_walltime();

    if (milton->current_mode != MiltonMode::PEEK_OUT) {
        milton_switch_mode(milton, MiltonMode::PEEK_OUT);
    }
}

void
peek_out_trigger_stop(Milton* milton)
{
    if (milton->current_mode == MiltonMode::PEEK_OUT && !milton->peek_out->peek_out_ended) {
        milton_set_zoom_at_point(milton, milton->hover_point);
        milton->peek_out->high_scale = milton_render_scale(milton);
        milton->peek_out->low_scale = milton->view->scale;
        milton->peek_out->begin_anim_time = platform_get_walltime();
        milton->peek_out->peek_out_ended = true;
    }
}

void
peek_out_tick(Milton* milton)
{
    if (milton->current_mode == MiltonMode::PEEK_OUT) {
        gpu_update_scale(milton->render_data, milton_render_scale(milton));
        if ( milton->peek_out->peek_out_ended &&
             difference_in_ms(milton->peek_out->begin_anim_time, platform_get_walltime()) > peek_out_duration_ms(milton) ) {
            milton_use_previous_mode(milton);
        }
    }
}

void
milton_update_and_render(Milton* milton, MiltonInput* input)
{
    PROFILE_GRAPH_BEGIN(update);

    b32 end_stroke = (input->flags & MiltonInputFlags_END_STROKE);

    milton->render_settings.do_full_redraw = false;

    b32 brush_outline_should_draw = false;
    int render_flags = RenderBackendFlags_NONE | RenderBackendFlags_WITH_BLUR;

    b32 draw_custom_rectangle = false;  // Custom rectangle used for new strokes, undo/redo.
    Rect custom_rectangle = rect_without_size();

    b32 should_save =
            ((input->flags & MiltonInputFlags_OPEN_FILE)) ||
            ((input->flags & MiltonInputFlags_SAVE_FILE)) ||
            ((input->flags & MiltonInputFlags_END_STROKE)) ||
            ((input->flags & MiltonInputFlags_UNDO)) ||
            ((input->flags & MiltonInputFlags_REDO));

    if ( input->flags & MiltonInputFlags_OPEN_FILE ) {
        milton_load(milton);
        upload_gui(milton);
        milton->render_settings.do_full_redraw = true;
        render_flags |= RenderBackendFlags_WITH_BLUR;
    }

    if ( milton->flags & MiltonStateFlags_FULL_REDRAW_REQUESTED ) {
        milton->flags &= ~MiltonStateFlags_FULL_REDRAW_REQUESTED;
        milton->render_settings.do_full_redraw = true;
    }

    i32 now = (i32)SDL_GetTicks();

    if ( input->flags & MiltonInputFlags_FULL_REFRESH ) {
        milton->render_settings.do_full_redraw = true;
        // GUI might have changed layer effect parameters.
        render_flags |= RenderBackendFlags_WITH_BLUR;
    }

    if ( input->scale ) {
        milton->render_settings.do_full_redraw = true;

        f32 scale_factor = SCALE_FACTOR;
        i32 view_scale_limit = VIEW_SCALE_LIMIT;

        i32 min_scale = MINIMUM_SCALE;

        if ( input->scale > 0 && milton->view->scale >= min_scale ) {
            milton->view->scale = (i32)(ceilf(milton->view->scale / scale_factor));
        }
        else if ( input->scale < 0 && milton->view->scale < view_scale_limit ) {
            milton->view->scale = (i32)(milton->view->scale * scale_factor) + 1;
        }

        milton_update_brushes(milton);
        gpu_update_scale(milton->render_data, milton->view->scale);
    }
    else if ( (input->flags & MiltonInputFlags_PANNING) ) {
        // If we are *not* zooming and we are panning, we can copy most of the
        // framebuffer
        if ( !(input->pan_delta == v2l{}) ) {
            milton->render_settings.do_full_redraw = true;
        }
    }

    if ( input->mode_to_set != milton->current_mode
         && mode_is_for_drawing(input->mode_to_set)) {
        end_stroke = true;
    }

    { // Undo / Redo
        if ( (input->flags & MiltonInputFlags_UNDO) ) {
            // Grab undo elements. They might be from deleted layers, so discard dead results.
            while ( milton->canvas->history.count > 0 ) {
                HistoryElement h = pop(&milton->canvas->history);
                Layer* l = layer::get_by_id(milton->canvas->root_layer, h.layer_id);
                // found a thing to undo.
                if ( l ) {
                    if ( l->strokes.count > 0 ) {
                        Stroke* stroke_ptr = peek(&l->strokes);
                        Stroke stroke = pop(&l->strokes);
                        push(&milton->canvas->stroke_graveyard, stroke);
                        push(&milton->canvas->redo_stack, h);

                        milton->render_settings.do_full_redraw = true;
                        render_flags |= RenderBackendFlags_WITH_BLUR;

                        SaveBlockHeader header = {};
                        header.type = Block_LAYER_CONTENT;
                        header.block_layer.id = h.layer_id;
                    }
                    break;
                }
            }
        }
        else if ( (input->flags & MiltonInputFlags_REDO) ) {
            if ( milton->canvas->redo_stack.count > 0 ) {
                HistoryElement h = pop(&milton->canvas->redo_stack);
                switch ( h.type ) {
                case HistoryElement_STROKE_ADD: {
                    Layer* l = layer::get_by_id(milton->canvas->root_layer, h.layer_id);
                    if ( l && count(&milton->canvas->stroke_graveyard) > 0 ) {
                        Stroke stroke = pop(&milton->canvas->stroke_graveyard);
                        if ( stroke.layer_id == h.layer_id ) {
                            push(&l->strokes, stroke);
                            push(&milton->canvas->history, h);

                            milton->render_settings.do_full_redraw = true;
                            render_flags |= RenderBackendFlags_WITH_BLUR;

                            SaveBlockHeader header = {};
                            header.type = Block_LAYER_CONTENT;
                            header.block_layer.id = h.layer_id;

                            break;
                        }

                        stroke = pop(&milton->canvas->stroke_graveyard);  // Keep popping in case the graveyard has info from deleted layers
                    }

                } break;
                /* case HistoryElement_LAYER_DELETE: { */
                /* } break; */
                }
            }
        }
    }

    // If the current mode is Pen or Eraser, we show the hover. It can be unset under various conditions later.
    if ( current_mode_is_for_drawing(milton) ) {
        brush_outline_should_draw = true;
    }

    if ( (input->flags & MiltonInputFlags_HOVERING) ) {
        milton->hover_point = input->hover_point;
    }

    if ( gui_point_hovers(milton->gui, milton->hover_point) ) {
        brush_outline_should_draw = false;
    }

    // Set the hover point as the last point of the working stroke.
    if ( is_user_drawing(milton) ) {
        i32 np = milton->working_stroke.num_points;
        if ( np > 0 ) {
            milton->hover_point = VEC2I(canvas_to_raster(milton->view, milton->working_stroke.points[np-1]));
        }
    }

    if ( input->input_count > 0 || (input->flags | MiltonInputFlags_CLICK) ) {
        if ( current_mode_is_for_drawing(milton) ) {
            if ( !is_user_drawing(milton)
                 && gui_consume_input(milton->gui, input) ) {
                milton_update_brushes(milton);
                gpu_update_picker(milton->render_data, &milton->gui->picker);
            }
            else if ( !milton->gui->owns_user_input
                      && (milton->canvas->working_layer->flags & LayerFlags_VISIBLE) ) {
                if ( milton->current_mode == MiltonMode::PRIMITIVE ) {
                    // Input for primitive.
                    milton_primitive_input(milton, input, end_stroke);
                }
                else {  // Input for eraser and pen
                    Stroke* ws = &milton->working_stroke;
                    auto prev_num_points = ws->num_points;
                    milton_stroke_input(milton, input);
                    if ( prev_num_points == 0 && ws->num_points > 0 ) {
                        // New stroke. Clear screen without blur.
                        milton->render_settings.do_full_redraw = true;
                    }
                }
            }
        }
    }

    if ( milton->current_mode == MiltonMode::EXPORTING ) {
        Exporter* exporter = &milton->gui->exporter;
        b32 changed = exporter_input(exporter, input);
        if ( changed ) {
            gpu_update_export_rect(milton->render_data, exporter);
        }
        if ( exporter->state != ExporterState_EMPTY ) {
             render_flags |= RenderBackendFlags_EXPORTING;
        }
        milton->gui->flags &= ~(MiltonGuiFlags_SHOWING_PREVIEW);
    }
    else if ( render_flags & RenderBackendFlags_EXPORTING ) {
        render_flags &= ~RenderBackendFlags_EXPORTING;
    }

    if ( (input->flags & MiltonInputFlags_IMGUI_GRABBED_INPUT) ) {
        // Start drawing the preview if we just grabbed a slider.
        brush_outline_should_draw = false;

        if ( (milton->gui->flags & MiltonGuiFlags_SHOWING_PREVIEW) ) {
            auto preview_pos = milton->gui->preview_pos;
            mlt_assert(preview_pos.x >= 0);
            mlt_assert(preview_pos.y >= 0);
            v4f color = {};
            color.rgb = milton->view->background_color;
            color.a = 1;
            if ( milton->current_mode == MiltonMode::PEN ) {
                color = to_premultiplied(hsv_to_rgb(milton->gui->picker.data.hsv),
                                         milton_get_brush_alpha(milton));
            }
            gpu_update_brush_outline(milton->render_data,
                                     preview_pos.x, preview_pos.y,
                                     milton_get_brush_radius(milton), BrushOutline_FILL, color);
        }
    } else {
        gui_imgui_set_ungrabbed(milton->gui);
    }

    if ( milton->gui->visible ) {
        render_flags |= RenderBackendFlags_GUI_VISIBLE;
    } else {
        render_flags &= ~RenderBackendFlags_GUI_VISIBLE;
    }

    if ( milton->current_mode == MiltonMode::EYEDROPPER ) {
        eyedropper_init(milton);
        v2i point = {};
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
            eyedropper_input(milton->eyedropper, milton->gui,
                             milton->view->screen_size.w,
                             milton->view->screen_size.h,
                             point);
            gpu_update_picker(milton->render_data, &milton->gui->picker);
        }
        if( input->flags & MiltonInputFlags_CLICKUP ) {
            if ( !(milton->flags & MiltonStateFlags_IGNORE_NEXT_CLICKUP) ) {
                milton_switch_mode(milton, MiltonMode::PEN);
                milton_update_brushes(milton);
            } else {
                milton->flags &= ~MiltonStateFlags_IGNORE_NEXT_CLICKUP;
            }
        }
    }

    // ---- End stroke
    if ( end_stroke ) {
        if ( milton->gui->owns_user_input ) {
            gui_deactivate(milton->gui);
            brush_outline_should_draw = false;
        } else {
            if ( milton->working_stroke.num_points > 0 ) {
                // We used the selected color to draw something. Push.
                if (  (milton->current_mode == MiltonMode::PEN ||
                       milton->current_mode == MiltonMode::PRIMITIVE)
                     && gui_mark_color_used(milton->gui) ) {
                    // Tell the renderer to update the picker
                    gpu_update_picker(milton->render_data, &milton->gui->picker);
                }
                // Copy current stroke.
                Stroke new_stroke = {};
                CanvasState* canvas = milton->canvas;
                copy_stroke(&canvas->arena, milton->view, &milton->working_stroke, &new_stroke);
                {
                    new_stroke.brush = milton->working_stroke.brush;
                    new_stroke.layer_id = milton->view->working_layer_id;
                    new_stroke.bounding_rect = rect_union(bounding_box_for_stroke(&new_stroke),
                                                        bounding_box_for_stroke(&new_stroke));

                    new_stroke.id = milton->canvas->stroke_id_count++;

                    draw_custom_rectangle = true;
                    Rect bounds = new_stroke.bounding_rect;
                    bounds.top_left = canvas_to_raster(milton->view, bounds.top_left);
                    bounds.bot_right = canvas_to_raster(milton->view, bounds.bot_right);
                    custom_rectangle = rect_union(custom_rectangle, bounds);
                }

                mlt_assert(new_stroke.num_points > 0);
                mlt_assert(new_stroke.num_points <= STROKE_MAX_POINTS);
                auto* stroke = layer::layer_push_stroke(milton->canvas->working_layer, new_stroke);

                // Invalidate working stroke render element

                HistoryElement h = { HistoryElement_STROKE_ADD, milton->canvas->working_layer->id };
                push(&milton->canvas->history, h);

                reset_working_stroke(milton);

                clear_stroke_redo(milton);

                // Make sure we show blurred layers when finishing a stroke.
                render_flags |= RenderBackendFlags_WITH_BLUR;
                milton->render_settings.do_full_redraw = true;

                // Update save block
                SaveBlockHeader header = {};
                header.type = Block_LAYER_CONTENT;
                header.block_layer.id = milton->view->working_layer_id;
            }
        }
    }
    else if ( is_user_drawing(milton) ) {
        Rect previous_bounds = milton->working_stroke.bounding_rect;
        Rect new_bounds = bounding_box_for_stroke(&milton->working_stroke);

        new_bounds.left = min(new_bounds.left, previous_bounds.left);
        new_bounds.top = min(new_bounds.top, previous_bounds.top);
        new_bounds.right = max(new_bounds.right, previous_bounds.right);
        new_bounds.bottom = max(new_bounds.bottom, previous_bounds.bottom);

        milton->working_stroke.bounding_rect  = new_bounds;
    }

    MiltonMode current_mode = milton->current_mode;
    if ( input->mode_to_set != MiltonMode::NONE ) {
        if ( current_mode == input->mode_to_set ) {
            // Modes we can toggle
            MiltonMode toggleable_modes[] = {
                MiltonMode::EYEDROPPER,
                MiltonMode::PRIMITIVE,
            };

            for ( size_t i = 0; i < array_count(toggleable_modes); ++i ) {
                MiltonMode toggle = toggleable_modes[i];
                if ( current_mode == toggle ) {
                    if ( milton->last_mode != toggle ) {
                        milton_use_previous_mode(milton);
                    }
                    else {
                        // This is not supposed to happen but if we get here we won't crash and burn.
                        milton_switch_mode(milton, MiltonMode::PEN);
                        milton_log("Warning: Unexpected code path: Toggling modes. Toggleable mode was set twice. Switching to pen.\n");
                    }
                }
            }
        }
        // Change the current mode if it's different from the mode to set
        else {
            milton_switch_mode(milton, input->mode_to_set);
            if (    input->mode_to_set == MiltonMode::PEN
                 || input->mode_to_set == MiltonMode::ERASER
                 || input->mode_to_set == MiltonMode::PRIMITIVE ) {
                milton_update_brushes(milton);
                // If we are drawing, end the current stroke so that it
                // doesn't change from eraser to brush or vice versa.
            }
        }
    }

    // Disable hover if panning.
    if ( input->flags & MiltonInputFlags_PANNING ) {
        brush_outline_should_draw = false;
    }

    #if MILTON_HARDWARE_BRUSH_CURSOR
        if ( milton_get_brush_radius(milton) < MILTON_HIDE_BRUSH_OVERLAY_AT_THIS_SIZE ) {
            brush_outline_should_draw = false;
        }
    #endif

    if ( !brush_outline_should_draw
         && (i32)SDL_GetTicks() - milton->hover_flash_ms < HOVER_FLASH_THRESHOLD_MS ) {
        brush_outline_should_draw = true;
    }


    if ( !(milton->gui->flags & MiltonGuiFlags_SHOWING_PREVIEW) ) {
        float radius = -1;
        if (brush_outline_should_draw && current_mode_is_for_drawing(milton)) {
            radius = (float)milton_get_brush_radius(milton);
        }
        else if (milton->current_mode == MiltonMode::PEEK_OUT) {
            radius = 100;  // TODO: Draw rectangle as cursor
        }

        gpu_update_brush_outline(milton->render_data,
                                milton->hover_point.x, milton->hover_point.y,
                                radius);
    }

    PROFILE_GRAPH_END(update);

    if ( !(milton->flags & MiltonStateFlags_RUNNING) ) {
        // Someone tried to kill milton from outside the update. Make sure we save.
        should_save = true;
        // Don't want to leave the system with the cursor hidden.
        platform_cursor_show();
    }

    if ( should_save ) {
        if ( !(milton->flags & MiltonStateFlags_RUNNING) ) {
            // Always save synchronously when exiting.
            milton_save(milton);
        } else {
#if MILTON_SAVE_ASYNC
            trigger_async_save(milton);
#else
            milton_save(milton);
#endif
        }
        // We're about to close and the last save failed and the drawing changed.
        if (    !(milton->flags & MiltonStateFlags_RUNNING)
             && (milton->flags & MiltonStateFlags_LAST_SAVE_FAILED)
             && (milton->flags & MiltonStateFlags_MOVE_FILE_FAILED)
             && milton->persist->last_save_stroke_count != layer::count_strokes(milton->canvas->root_layer) ) {
            // TODO: Stop using MoveFileEx?
            //  Why does MoveFileEx fail? Ask someone who knows this stuff.
            // Wait a moment and try again. If this fails, prompt to save somewhere else.
            SDL_Delay(3000);
            milton_save(milton);

            if (    (milton->flags & MiltonStateFlags_LAST_SAVE_FAILED)
                 && (milton->flags & MiltonStateFlags_MOVE_FILE_FAILED) ) {
                char msg[1024];
                WallTime lst = milton->persist->last_save_time;
                snprintf(msg, 1024, "Milton failed to save this canvas. The last successful save was at %.2d:%.2d:%.2d. Try saving to another file?",
                         lst.hours, lst.minutes, lst.seconds);
                b32 another = platform_dialog_yesno(msg, "Try another file?");
                if ( another ) {
                    // NOTE(possible refactor): There is similar code. Guipp.cpp save_milton_canvas
                    PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                    if ( name ) {
                        milton_log("Saving to %s\n", name);
                        milton_set_canvas_file(milton, name);
                        milton_save(milton);
                        if ( (milton->flags & MiltonStateFlags_LAST_SAVE_FAILED) ) {
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


        // About to quit.
        if ( !(milton->flags & MiltonStateFlags_RUNNING) ) {
            milton_kill_save_thread(milton);
            // Release resources
            milton_reset_canvas(milton);
            gpu_release_data(milton->render_data);

            debug_memory_dump_allocations();
        }
    }

#if MILTON_SAVE_ASYNC
    SDL_LockMutex(milton->save_mutex);
    SDL_CondSignal(milton->save_cond);
    SDL_UnlockMutex(milton->save_mutex);
#endif

    // Update render resources after loading
    if (milton->flags & MiltonStateFlags_JUST_SAVED) {
        milton_set_background_color(milton, milton->view->background_color);
        gpu_update_picker(milton->render_data, &milton->gui->picker);
        milton->flags &= ~MiltonStateFlags_JUST_SAVED;
    }

    if (milton->current_mode == MiltonMode::PEEK_OUT) {
        milton->render_settings.do_full_redraw = true;
        peek_out_tick(milton);
    }

    i32 view_x = 0;
    i32 view_y = 0;
    i32 view_width = 0;
    i32 view_height = 0;

    gpu_reset_render_flags(milton->render_data, render_flags);

    ClipFlags clip_flags = ClipFlags_JUST_CLIP;

#if REDRAW_EVERY_FRAME
    milton->render_settings.do_full_redraw = true;
#endif

    // Note: We flip the rectangles. GL is bottom-left by default.
    if ( milton->render_settings.do_full_redraw ) {
        view_width = milton->view->screen_size.w;
        view_height = milton->view->screen_size.h;
        // Only update GPU data if we are redrawing the full screen. This means
        // that the size of the screen will be used to determine if each stroke
        // should be freed from GPU memory.
        clip_flags = ClipFlags_UPDATE_GPU_DATA;
    }
    else if ( draw_custom_rectangle ) {
        view_x = custom_rectangle.left;
        view_y = custom_rectangle.top;
        view_width = custom_rectangle.right - custom_rectangle.left;
        view_height = custom_rectangle.bottom - custom_rectangle.top;
        VALIDATE_RECT(custom_rectangle);
    }
    else if ( milton->working_stroke.num_points > 0 ) {
        Rect bounds      = milton->working_stroke.bounding_rect;
        bounds.top_left  = canvas_to_raster(milton->view, bounds.top_left);
        bounds.bot_right = canvas_to_raster(milton->view, bounds.bot_right);

        view_x           = bounds.left;
        view_y           = bounds.top;

        view_width  = bounds.right - bounds.left;
        view_height = bounds.bottom - bounds.top;
    }

    PROFILE_GRAPH_BEGIN(clipping);

    i64 render_scale = milton_render_scale(milton);

    gpu_clip_strokes_and_update(&milton->root_arena, milton->render_data, milton->view, render_scale,
                                milton->canvas->root_layer, &milton->working_stroke,
                                view_x, view_y, view_width, view_height, clip_flags);
    PROFILE_GRAPH_END(clipping);

    gpu_render(milton->render_data, view_x, view_y, view_width, view_height);

    ARENA_VALIDATE(&milton->root_arena);
}
