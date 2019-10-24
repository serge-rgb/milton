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
    gpu_update_background(milton->renderer, milton->view->background_color);
}

static void
init_view(CanvasView* view, v3f background_color, i32 width, i32 height)
{
    milton_log("Setting default view\n");

    auto size = view->screen_size;

    *view = CanvasView{};

    view->size = sizeof(CanvasView);
    view->background_color  = background_color;
    view->screen_size       = size;
    view->zoom_center       = size / 2;
    view->scale             = MILTON_DEFAULT_SCALE;
    view->angle             = 0.0f;
    view->screen_size       = { width, height };
}

int
milton_get_brush_enum(Milton const* milton)
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
        case MiltonMode::DRAG_BRUSH_SIZE: {
            brush_enum = milton->drag_brush->brush_idx;
        } break;
        case MiltonMode::EXPORTING:
        case MiltonMode::EYEDROPPER:
        case MiltonMode::HISTORY:
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
        }
        else if ( i == BrushEnum_ERASER ) {
            // Nothing
        }
        else if ( i == BrushEnum_PRIMITIVE ) {
            brush->color = to_premultiplied(gui_get_picker_rgb(milton->gui), brush->alpha);
        }
    }
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
milton_get_brush(Milton const* milton)
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
is_user_drawing(Milton const* milton)
{
    b32 result = milton->working_stroke.num_points > 0;
    return result;
}

static b32
mode_is_for_drawing(MiltonMode mode)
{
    b32 result = mode == MiltonMode::PEN ||
            mode == MiltonMode::ERASER ||
            mode == MiltonMode::PRIMITIVE ||
            mode == MiltonMode::DRAG_BRUSH_SIZE;
    return result;
}

static size_t
get_gui_visibility_index(Milton* milton)
{
    size_t idx = Milton::GuiVisibleCategory_OTHER;
    if (current_mode_is_for_drawing(milton)) {
        idx = Milton::GuiVisibleCategory_DRAWING;
    }
    else if (milton->current_mode == MiltonMode::EXPORTING) {
        idx = Milton::GuiVisibleCategory_EXPORTING;
    }
    return idx;
}

void
milton_toggle_gui_visibility(Milton* milton)
{
    size_t idx = get_gui_visibility_index(milton);
    milton->mode_gui_visibility[idx] = !milton->mode_gui_visibility[idx];
}

void
milton_set_gui_visibility(Milton* milton, b32 visible)
{
    size_t idx = get_gui_visibility_index(milton);
    milton->mode_gui_visibility[idx] = visible;
}

b32
current_mode_is_for_drawing(Milton const* milton)
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
milton_primitive_input(Milton* milton, MiltonInput const* input, b32 end_stroke)
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
    v2f point = v2l_to_v2f(input - filter->center);

    if (filter->first)
    {
        filter->prediction = point;
        filter->first = false;
    }
    else
    {
        f32 alpha = 0.5;

        filter->prediction = alpha * point + (1 - alpha) * filter->prediction;
    }
    v2l result = v2f_to_v2l(filter->prediction) + filter->center;
    return result;
}

static void
clear_smooth_filter(SmoothFilter* filter, v2l center)
{
    filter->first = true;
    filter->center = center;
}

static u64 peek_out_duration_ms(Milton* milton);  // forward decl

static float
peek_out_interpolation(Milton* milton)
{
    float interp = 0.0f;

    WallTime time = platform_get_walltime();
    u64 ms = difference_in_ms(milton->peek_out->begin_anim_time, time);

    if (ms < peek_out_duration_ms(milton)) {
        interp = ms / (float)peek_out_duration_ms(milton);
    }
    else {
        interp = 1.0f;
    }

    if (milton->peek_out->peek_out_ended) {
        interp = 1 - interp;
    }
    return interp;
}

i64
milton_render_scale_with_interpolation(Milton* milton, float interp)
{
    if ( milton->current_mode == MiltonMode::PEEK_OUT ) {
        return lerp(milton->peek_out->low_scale, milton->peek_out->high_scale, interp);
    }
    else {
        return milton->view->scale;
    }
}

i64
milton_render_scale(Milton* milton)
{
    float interp = 1.0f;
    if (milton->current_mode == MiltonMode::PEEK_OUT) {
        interp = peek_out_interpolation(milton);
    }
    return milton_render_scale_with_interpolation(milton, interp);
}

static void
milton_stroke_input(Milton* milton, MiltonInput const* input)
{
    if ( input->input_count == 0 ) {
        return;
    }

    if ( milton->view->scale != milton_render_scale(milton) ) {
        return;  // We can't draw while peeking.
    }

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
milton_set_zoom_at_point_with_scale(Milton* milton, v2i new_zoom_center, i64 scale)
{
    milton->view->pan_center = raster_to_canvas_with_scale(milton->view, v2i_to_v2l(new_zoom_center), scale);
    milton->view->zoom_center = new_zoom_center;
    gpu_update_canvas(milton->renderer, milton->canvas, milton->view);
}

void
milton_set_zoom_at_point(Milton* milton, v2i new_zoom_center)
{
    milton_set_zoom_at_point_with_scale(milton, new_zoom_center, milton_render_scale(milton));
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

static i32
milton_get_brush_radius_for_enum(Milton const* milton, int brush_enum)
{
    i32 brush_size = milton->brush_sizes[brush_enum];
    if ( brush_size <= 0 ) {
        brush_size = 1;
    }
    return brush_size;
}

i32
milton_get_brush_radius(Milton const* milton)
{
    i32 radius = milton_get_brush_radius_for_enum(milton, milton_get_brush_enum(milton));
    return radius;
}

void
milton_set_brush_size_for_enum(Milton* milton, i32 size, int brush_idx)
{
    if ( current_mode_is_for_drawing(milton) ) {
        (*pointer_to_brush_size(milton)) = size;
        milton_update_brushes(milton);
    }
}

void
milton_set_brush_size(Milton* milton, i32 size)
{
    milton_set_brush_size_for_enum(milton, size, milton_get_brush_enum(milton));
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
    milton->brushes[brush_enum].pressure_opacity_min = min(alpha, milton->brushes[brush_enum].pressure_opacity_min);
    milton_update_brushes(milton);
}

float
milton_get_brush_alpha(Milton const* milton)
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
    gpu_reset_stroke(milton->renderer, milton->working_stroke.render_handle);
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

    milton->renderer = gpu_allocate_render_backend(&milton->root_arena);

    milton->smooth_filter = arena_alloc_elem(&milton->root_arena, SmoothFilter);

    if (init_graphics) { milton->gl = arena_alloc_elem(&milton->root_arena, MiltonGLState); }
    milton->gui = arena_alloc_elem(&milton->root_arena, MiltonGui);
    milton->settings = arena_alloc_elem(&milton->root_arena, MiltonSettings);
    milton->eyedropper = arena_alloc_elem(&milton->root_arena, Eyedropper);
    milton->persist = arena_alloc_elem(&milton->root_arena, MiltonPersist);
    milton->drag_brush = arena_alloc_elem(&milton->root_arena, MiltonDragBrush);
    milton->transform = arena_alloc_elem(&milton->root_arena, TransformMode);

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

    init_view(milton->view, milton->settings->background_color, width, height);
    if (init_graphics) { gpu_init(milton->renderer, milton->view, &milton->gui->picker); }

    if (init_graphics) { gpu_update_background(milton->renderer, milton->view->background_color); }

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

    // Set default brush.
    {
        for ( int i = 0; i < BrushEnum_COUNT; ++i ) {

            milton->brushes[i].alpha = 1.0f;
            milton->brushes[i].hardness = k_max_hardness;

            switch ( i ) {
            case BrushEnum_PEN: {
                milton->brush_sizes[i] = 30;
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
    }

    milton->persist->last_save_time = {};
    // Note: This will fill out uninitialized data like default layers.
    if (read_from_disk) { milton_load(milton); }

    milton_validate(milton);

    // Enable brush smoothing by default
    if ( !milton_brush_smoothing_enabled(milton) ) {
        milton_toggle_brush_smoothing(milton);
    }

    // Default mode GUI visibility
    milton->mode_gui_visibility[Milton::GuiVisibleCategory_DRAWING] = true;

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
        gpu_update_canvas(milton->renderer, milton->canvas, milton->view);
        gpu_resize(milton->renderer, milton->view);
        gpu_update_picker(milton->renderer, &milton->gui->picker);
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

        f32 x = pan_delta.x;
        f32 y = pan_delta.y;

        f32 cos_angle = cosf(milton->view->angle);
        f32 sin_angle = sinf(milton->view->angle);

        v2f deltaf = v2f{x * cos_angle - y * sin_angle, y * cos_angle + x * sin_angle };
        // Add delta to pan vector
        v2l pan_center = milton->view->pan_center - (v2f_to_v2l(deltaf) * milton->view->scale);

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

    gpu_free_strokes(milton->renderer, milton->canvas);
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
    init_view(milton->view,
        milton->settings->background_color,
        milton->view->screen_size.x,
        milton->view->screen_size.y);
    milton->view->background_color = milton->settings->background_color;
    gpu_update_background(milton->renderer, milton->view->background_color);

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

static void
push_mode(Milton* milton, MiltonMode mode)
{
    if (milton->n_mode_stack < MODE_STACK_MAX) {
        milton->mode_stack[ milton->n_mode_stack++ ] = mode;
    }
}

static MiltonMode
pop_mode(Milton* milton)
{
    MiltonMode result = MiltonMode::PEN;
    if (milton->n_mode_stack) {
        result = milton->mode_stack[ --milton->n_mode_stack ];
    }
    return result;
}

MiltonMode
milton_leave_mode(Milton* milton)
{
    if (milton->current_mode == MiltonMode::EYEDROPPER) {
        eyedropper_deinit(milton->eyedropper);
    }
    MiltonMode leaving = milton->current_mode;
    milton->current_mode = pop_mode(milton);
    return leaving;
}

void
milton_enter_mode(Milton* milton, MiltonMode mode)
{
    if ( mode != milton->current_mode ) {
        if (mode == MiltonMode::EYEDROPPER) {
            eyedropper_init(milton);
        }
        push_mode(milton, milton->current_mode);
        milton->current_mode = mode;

        if (is_user_drawing(milton)) {
            milton->flags |= MiltonStateFlags_FINISH_CURRENT_STROKE;
        }
    }
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

    out_stroke->render_handle = 0;
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
peek_out_trigger_start(Milton* milton, int peek_out_flags)
{
    milton_set_zoom_at_screen_center(milton);
    milton->peek_out->begin_pan = milton->view->pan_center;

    milton->peek_out->low_scale = milton_render_scale(milton);
    milton->peek_out->high_scale = peek_out_target_scale(milton);
    milton->peek_out->peek_out_ended = false;
    milton->peek_out->begin_anim_time = platform_get_walltime();
    milton->peek_out->flags = peek_out_flags;

    if (milton->current_mode != MiltonMode::PEEK_OUT) {
        milton_enter_mode(milton, MiltonMode::PEEK_OUT);
    }
}

void
peek_out_trigger_stop(Milton* milton)
{
    if (milton->current_mode == MiltonMode::PEEK_OUT && !milton->peek_out->peek_out_ended) {
        milton_set_zoom_at_point(milton, milton->platform->pointer);

        i64 scale = milton_render_scale(milton);
        milton->peek_out->high_scale = scale;
        milton->peek_out->low_scale = milton->view->scale;
        milton->peek_out->begin_anim_time = platform_get_walltime();
        milton->peek_out->peek_out_ended = true;
        milton->peek_out->end_pan = raster_to_canvas_with_scale(milton->view, v2i_to_v2l(milton->platform->pointer), scale);
    }
}

static void
peek_out_tick(Milton* milton, MiltonInput const* input)
{
    PeekOut* peek = milton->peek_out;

    if (milton->current_mode == MiltonMode::PEEK_OUT) {

        float interp = peek_out_interpolation(milton);

        if ((peek->flags & PeekOut_CLICK_TO_EXIT) && input->input_count > 0)
        {
            peek_out_trigger_stop(milton);
        }

        if ( milton->peek_out->peek_out_ended ) {
            i64 panx = lerp(peek->end_pan.x, peek->begin_pan.x, interp);
            i64 pany = lerp(peek->end_pan.y, peek->begin_pan.y, interp);

            v2l pan = { panx, pany };

            milton->view->pan_center = pan;
            milton->view->zoom_center = milton->view->screen_size / 2;
            gpu_update_canvas(milton->renderer, milton->canvas, milton->view);

            if ( difference_in_ms(peek->begin_anim_time, platform_get_walltime()) > peek_out_duration_ms(milton) ) {
                milton_leave_mode(milton);
            }
        }
        gpu_update_scale(milton->renderer, milton_render_scale(milton));

        {
            i32 width = 100;
            i32 height = 50;
            i32 line_width = 2;

            // TODO: Interpolate pan vector
            f32 scale = (f32)milton_render_scale_with_interpolation(milton, interp);

            f32 cx = 2 * milton->platform->pointer.x / (f32)milton->view->screen_size.w - 1;
            f32 cy = (2 * milton->platform->pointer.y / (f32)milton->view->screen_size.h - 1)*-1;
            f32 left = cx - milton->view->scale / scale;
            f32 right = cx + milton->view->scale / scale;
            f32 top = cy - milton->view->scale / scale;
            f32 bottom = cy + milton->view->scale / scale;

            imm_rect(milton->renderer, left, right, top, bottom, line_width);
        }
    }
}

void
drag_brush_size_start(Milton* milton, v2i pointer)
{
    if ( milton->current_mode != MiltonMode::DRAG_BRUSH_SIZE &&
         current_mode_is_for_drawing(milton) ) {
        milton->drag_brush->brush_idx = milton_get_brush_enum(milton);
        i32 size = milton_get_brush_radius(milton);
        milton->drag_brush->start_point = platform_cursor_get_position(milton->platform);
        milton->drag_brush->start_size = size;
        milton_enter_mode(milton, MiltonMode::DRAG_BRUSH_SIZE);
    }
}

void
drag_brush_size_stop(Milton* milton)
{
    if (milton->current_mode == MiltonMode::DRAG_BRUSH_SIZE) {
        v2i point = milton->drag_brush->start_point;
        platform_cursor_set_position(milton->platform, point);
        milton_leave_mode(milton);
    }
}

static void
drag_brush_size_tick(Milton* milton, MiltonInput const* input)
{
    MiltonDragBrush* drag = milton->drag_brush;
    f32 drag_factor = 0.5f;
    i64 mouse_x = platform_cursor_get_position(milton->platform).x;

    f32 new_size = drag->start_size + drag_factor * (mouse_x - drag->start_point.x);
    if ( new_size < 1 )
        new_size = 1;
    if ( new_size > 300 )
        new_size = 300;
    milton_set_brush_size_for_enum(milton, static_cast<i32>(new_size), drag->brush_idx);
    milton_update_brushes(milton);
}

void
transform_start(Milton* milton, v2i pointer)
{
    if (milton->current_mode != MiltonMode::TRANSFORM) {
        milton_enter_mode(milton, MiltonMode::TRANSFORM);
        milton_set_zoom_at_screen_center(milton);
    }
}

void
transform_stop(Milton* milton)
{
    if (milton->current_mode == MiltonMode::TRANSFORM) {
        milton_leave_mode(milton);
    }
}

static void
transform_tick(Milton* milton, MiltonInput const* input)
{
    TransformMode* t = milton->transform;

    if (input->input_count > 0) {
        v2f point = v2l_to_v2f(input->points[ input->input_count - 1 ]);
        if (t->fsm == TransformModeFSM::START) {
            t->fsm = TransformModeFSM::ROTATING;
            t->last_point = point;
        }
        else if (t->fsm == TransformModeFSM::ROTATING) {
            // Rotate!
            const v2f center = v2i_to_v2f(milton->view->screen_size / 2);
            const v2f a = point - center;
            const v2f b = t->last_point - center;
            const f32 lena = magnitude(a);
            const f32 lenb = magnitude(b);
            if (lena > 0.0f && lenb > 0.0f) {
                const f32 cos_angle = clamp(DOT(b, a) / (lena * lenb), 0.0f, 1.0f);
                const f32 sign = orientation(center, t->last_point, point) > 0 ? -1.0f : 1.0f;
                milton->view->angle += sign * acosf(cos_angle);
                t->last_point = point;
                gpu_update_canvas(milton->renderer, milton->canvas, milton->view);
            }
        }
    }
    if (input->flags & MiltonInputFlags_CLICKUP) {
        if (t->fsm == TransformModeFSM::ROTATING) {
            t->fsm = TransformModeFSM::START;
        }
    }
}

void
milton_update_and_render(Milton* milton, MiltonInput const* input)
{
    imm_begin_frame(milton->renderer);

    PROFILE_GRAPH_BEGIN(update);

    b32 end_stroke = (input->flags & MiltonInputFlags_END_STROKE) || (milton->flags & MiltonStateFlags_FINISH_CURRENT_STROKE);

    milton->flags &= ~MiltonStateFlags_FINISH_CURRENT_STROKE;

    milton->render_settings.do_full_redraw = false;

    b32 brush_outline_should_draw = false;
    int render_flags = RenderBackendFlags_NONE;

    b32 draw_custom_rectangle = false;  // Custom rectangle used for new strokes, undo/redo.

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
    }

    i32 now = (i32)SDL_GetTicks();

    // Set GUI visibility
    {
        size_t idx = get_gui_visibility_index(milton);

        milton->gui->visible = milton->mode_gui_visibility[idx];
    }

    if ( input->flags & MiltonInputFlags_FULL_REFRESH ) {
        milton->render_settings.do_full_redraw = true;
    }

    if ( input->scale ) {
        milton->render_settings.do_full_redraw = true;

        f32 scale_factor = SCALE_FACTOR;
        i32 view_scale_limit = VIEW_SCALE_LIMIT;

        i32 min_scale = MINIMUM_SCALE;

        // Update the current brush if it's canvas-relative
        if (milton->working_stroke.flags & StrokeFlag_RELATIVE_TO_CANVAS) {

            i32* psize = pointer_to_brush_size(milton);

            if ( input->scale > 0 && milton->view->scale >= min_scale ) {
                (*psize) = (i32)((*psize) * scale_factor) + 1;
            }
            else if ( input->scale < 0 && (*psize) < view_scale_limit ) {
                (*psize) = (i32)(ceilf((*psize) / scale_factor));
            }
        }

        if ( input->scale > 0 && milton->view->scale >= min_scale ) {
            milton->view->scale = (i32)(ceilf(milton->view->scale / scale_factor));
        }
        else if ( input->scale < 0 && milton->view->scale < view_scale_limit ) {
            milton->view->scale = (i32)(milton->view->scale * scale_factor) + 1;
        }

        milton_update_brushes(milton);
        gpu_update_scale(milton->renderer, milton->view->scale);
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


    if ( gui_point_hovers(milton->gui, milton->platform->pointer) ) {
        brush_outline_should_draw = false;
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
            gpu_update_brush_outline(milton->renderer,
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

    // Mode tick
    if (milton->current_mode == MiltonMode::ERASER) {
        milton->working_stroke.flags |= StrokeFlag_ERASER;
    }
    else {
        milton->working_stroke.flags &= ~StrokeFlag_ERASER;
    }
    if ( current_mode_is_for_drawing(milton) &&
        (input->input_count > 0 || end_stroke) ) {
        if ( !is_user_drawing(milton)
             && gui_consume_input(milton->gui, input) ) {
            milton_update_brushes(milton);
            gpu_update_picker(milton->renderer, &milton->gui->picker);
        }
        else if ( !milton->gui->owns_user_input
                  && (milton->canvas->working_layer->flags & LayerFlags_VISIBLE) ) {
            if ( milton->current_mode == MiltonMode::PRIMITIVE ) {
                // Input for primitive.
                milton_primitive_input(milton, input, end_stroke);
            }
            else if ( milton->current_mode != MiltonMode::DRAG_BRUSH_SIZE )  {  // Input for eraser and pen
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

    if ( milton->current_mode == MiltonMode::EXPORTING ) {
        Exporter* exporter = &milton->gui->exporter;
        b32 changed = exporter_input(exporter, input);

        {
            i32 x = min(exporter->pivot.x, exporter->needle.x);
            i32 y = min(exporter->pivot.y, exporter->needle.y);
            i32 w = MLT_ABS(exporter->pivot.x - exporter->needle.x);
            i32 h = MLT_ABS(exporter->pivot.y - exporter->needle.y);

            float left = 2*((float)    x / (milton->view->screen_size.w))-1;
            float right = 2*((GLfloat)(x+w) / (milton->view->screen_size.w))-1;
            float top = -(2*((GLfloat)y     / (milton->view->screen_size.h))-1);
            float bottom = -(2*((GLfloat)(y+h) / (milton->view->screen_size.h))-1);

            imm_rect(milton->renderer, left, right, top, bottom, 2.0);
        }

        milton->gui->flags &= ~(MiltonGuiFlags_SHOWING_PREVIEW);
    }
    else if ( milton->current_mode == MiltonMode::EYEDROPPER ) {
        v2i point = milton->platform->pointer;

        eyedropper_input(milton->eyedropper, milton->gui,
                         milton->view->screen_size.w,
                         milton->view->screen_size.h,
                         point);
        gpu_update_picker(milton->renderer, &milton->gui->picker);
        if( input->flags & MiltonInputFlags_CLICKUP ) {
            milton_update_brushes(milton);
            milton_leave_mode(milton);
        }
        render_flags |= RenderBackendFlags_GUI_VISIBLE;
    }
    else if (milton->current_mode == MiltonMode::PEEK_OUT) {
        peek_out_tick(milton, input);
    }
    else if (milton->current_mode == MiltonMode::DRAG_BRUSH_SIZE) {
        drag_brush_size_tick(milton, input);
    }
    else if (milton->current_mode == MiltonMode::TRANSFORM) {
        transform_tick(milton, input);
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
                    gpu_update_picker(milton->renderer, &milton->gui->picker);
                }
                // Copy current stroke.
                Stroke new_stroke = {};
                CanvasState* canvas = milton->canvas;
                copy_stroke(&canvas->arena, milton->view, &milton->working_stroke, &new_stroke);
                {
                    new_stroke.layer_id = milton->view->working_layer_id;
                    new_stroke.bounding_rect = bounding_box_for_stroke(&new_stroke);

                    new_stroke.id = milton->canvas->stroke_id_count++;

                    Rect bounds = new_stroke.bounding_rect;
                    bounds.top_left = canvas_to_raster(milton->view, bounds.top_left);
                    bounds.bot_right = canvas_to_raster(milton->view, bounds.bot_right);
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
                milton->render_settings.do_full_redraw = true;
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
    if ( input->mode_to_set < MiltonMode::MODE_COUNT ) {
        if ( current_mode == input->mode_to_set ) {
            // Modes we can toggle
            MiltonMode toggleable_modes[] = {
                MiltonMode::EYEDROPPER,
                MiltonMode::PRIMITIVE,
            };

            for ( size_t i = 0; i < array_count(toggleable_modes); ++i ) {
                MiltonMode toggle = toggleable_modes[i];
                if ( current_mode == toggle ) {
                    if ( milton->n_mode_stack > 0 && milton->mode_stack[milton->n_mode_stack - 1] != toggle ) {
                        milton_leave_mode(milton);
                    }
                    else {
                        // This is not supposed to happen but if we get here we won't crash and burn.
                        milton_enter_mode(milton, MiltonMode::PEN);
                        milton_log("Warning: Unexpected code path: Toggling modes. Toggleable mode was set twice. Switching to pen.\n");
                    }
                }
            }
        }
        // Change the current mode if it's different from the mode to set
        else {
            if ( mode_is_for_drawing(input->mode_to_set) ) {
                milton_update_brushes(milton);

                // There is no way of leaving a drawing mode to some previous
                // mode. Therefore, When entering a drawing mode we clear the
                // mode stack.
                while ( milton->n_mode_stack ) {
                    milton_leave_mode(milton);
                }
            }
            milton_enter_mode(milton, input->mode_to_set);
        }
    }

    // Disable hover if panning.
    if ( input->flags & MiltonInputFlags_PANNING ) {
        brush_outline_should_draw = false;
    }

    if ( !(milton->gui->flags & MiltonGuiFlags_SHOWING_PREVIEW) ) {
        float radius = -1;
        if (brush_outline_should_draw && current_mode_is_for_drawing(milton)) {
            radius = (float)milton_get_brush_radius(milton);
        }

        v2i brush_point = milton->platform->pointer;
        if ( milton->current_mode == MiltonMode::DRAG_BRUSH_SIZE ) {
            brush_point = milton->drag_brush->start_point;
        }

        gpu_update_brush_outline(milton->renderer,
                                brush_point.x, brush_point.y,
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
            gpu_release_data(milton->renderer);

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
        gpu_update_picker(milton->renderer, &milton->gui->picker);
        milton->flags &= ~MiltonStateFlags_JUST_SAVED;
    }

    i32 view_x = 0;
    i32 view_y = 0;
    i32 view_width = 0;
    i32 view_height = 0;

    gpu_reset_render_flags(milton->renderer, render_flags);

    ClipFlags clip_flags = ClipFlags_JUST_CLIP;

#if REDRAW_EVERY_FRAME
    milton->render_settings.do_full_redraw = true;
#endif

    b32 has_working_stroke = milton->working_stroke.num_points > 0;

    if (has_working_stroke) {
        b32 has_blur = false;

        Layer* layer = milton->canvas->root_layer;
        while (layer) {
            if (layer->flags & LayerFlags_VISIBLE) {
                LayerEffect* e = layer->effects;
                while (e) {
                    if (e->enabled && e->type == LayerEffectType_BLUR) {
                        has_blur = true;
                        break;
                    }
                    e = e->next;
                }
            }
            if (has_blur) { break; }
            layer = layer->next;
        }

        if (has_blur) {
            milton->render_settings.do_full_redraw = true;
        }
    }

    static u64 scale_of_last_full_redraw = 0;
    static f32 angle_of_last_full_redraw = 0.0f;

    if (scale_of_last_full_redraw != milton_render_scale(milton) ||
        angle_of_last_full_redraw != milton->view->angle ) {
        // We want to draw everything when we're scaling up and down.
        milton->render_settings.do_full_redraw = true;
    }

    // Note: We flip the rectangles. GL is bottom-left by default.
    if ( milton->render_settings.do_full_redraw ) {
        view_width = milton->view->screen_size.w;
        view_height = milton->view->screen_size.h;
        // Only update GPU data if we are redrawing the full screen. This means
        // that the size of the screen will be used to determine if each stroke
        // should be freed from GPU memory.
        clip_flags = ClipFlags_UPDATE_GPU_DATA;
        scale_of_last_full_redraw = milton_render_scale(milton);
        angle_of_last_full_redraw = milton->view->angle;
    }
    else if (has_working_stroke) {
        Rect bounds  = canvas_to_raster_bounding_rect(milton->view, milton->working_stroke.bounding_rect);

        view_x           = bounds.left;
        view_y           = bounds.top;

        view_width  = bounds.right - bounds.left;
        view_height = bounds.bottom - bounds.top;
    }

    PROFILE_GRAPH_BEGIN(clipping);

    i64 render_scale = milton_render_scale(milton);

    gpu_clip_strokes_and_update(&milton->root_arena, milton->renderer, milton->view, render_scale,
                                milton->canvas->root_layer, &milton->working_stroke,
                                view_x, view_y, view_width, view_height, clip_flags);
    PROFILE_GRAPH_END(clipping);

    gpu_render(milton->renderer, view_x, view_y, view_width, view_height);

    ARENA_VALIDATE(&milton->root_arena);
}
