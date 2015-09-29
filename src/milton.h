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


#ifdef __cplusplus
extern "C"
{
#endif

// ==== Order matters. ====
//
//      We are compiling a big-blob of C code. Not multiple files. Everything
//      is #include'd only once.
//
//      There are only two places where we can add #include directives:
//          - This file.
//          - Platform-dependent *.c files.
//          - Be crystal clear on why these rules are broken when they are
//            broken. (if ever)
//
//      Every other file should assume that it knows about every function it
//      needs to call and every struct it uses. It is our responsibility to
//      include things in the right order in this file to make the program
//      compile.
//
// ========================

#include "define_types.h"

int milton_main();

#include "vector.generated.h"


typedef struct TabletState_s TabletState;


// These two defined on a per-platform basis
func void milton_fatal(char* message);
func void milton_die_gracefully(char* message);

#if defined(_WIN32)
#include "platform_windows.h"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.h"
#endif

#define MILTON_USE_VAO          0
#define RENDER_QUEUE_SIZE       (1 << 13)
#define STROKE_MAX_POINTS       2048
#define MAX_BRUSH_SIZE          80
#define MILTON_DEFAULT_SCALE    (1 << 10)
#define NO_PRESSURE_INFO        -1.0f
#define MAX_INPUT_BUFFER_ELEMS  32

#include "gl_helpers.h"

#include "memory.h"
#include "utils.h"
#include "color.h"
#include "canvas.h"


typedef struct MiltonGLState_s {
    GLuint quad_program;
    GLuint texture;
    GLuint vbo;
#if MILTON_USE_VAO
    GLuint quad_vao;
#endif
} MiltonGLState;

typedef enum MiltonMode_s {
    MiltonMode_NONE                   = ( 0 ),

    MiltonMode_ERASER                 = ( 1 << 0 ),
    MiltonMode_PEN                    = ( 1 << 1 ),
    MiltonMode_REQUEST_QUALITY_REDRAW = ( 1 << 2 ),
} MiltonMode;


// Render Workers:
//    We have a bunch of workers running on threads, who wait on a lockless
//    queue to take BlockgroupRenderData structures.
//    When there is work available, they call blockgroup_render_thread with the
//    appropriate parameters.
typedef struct BlockgroupRenderData_s {
    i32     block_start;
} BlockgroupRenderData;

typedef struct RenderQueue_s {
    Rect*   blocks;  // Screen areas to render.
    i32     num_blocks;
    u32*    raster_buffer;

    // FIFO work queue
    SDL_mutex*              mutex;
    BlockgroupRenderData    blockgroup_render_data[RENDER_QUEUE_SIZE];
    i32                     index;

    SDL_sem*   work_available;
    SDL_sem*   completed_semaphore;
} RenderQueue;

enum {
    BrushEnum_PEN,
    BrushEnum_ERASER,

    BrushEnum_COUNT,
};

typedef struct MiltonGui_s MiltonGui;

typedef struct MiltonState_s {
    u8      bytes_per_pixel;

    i32     max_width;
    i32     max_height;
    u8*     raster_buffer;

    // The screen is rendered in blockgroups
    // Each blockgroup is rendered in blocks of size (block_width*block_width).
    i32     blocks_per_blockgroup;
    i32     block_width;

    MiltonGLState* gl;

    MiltonGui* gui;

    Brush   brushes[BrushEnum_COUNT];
    i32     brush_sizes[BrushEnum_COUNT];  // In screen pixels

    CanvasView* view;
    v2i     hover_point;  // Track the pointer when not stroking..

    Stroke  working_stroke;

    StrokeCord* strokes;

    i32     num_redos;

    MiltonMode current_mode;

    i32             num_render_workers;
    RenderQueue*    render_queue;


    // Heap
    Arena*      root_arena;         // Persistent memory.
    Arena*      transient_arena;    // Gets reset after every call to milton_update().
    Arena*      render_worker_arenas;

    i32         worker_memory_size;
    b32         worker_needs_memory;

    b32         cpu_has_sse2;
    b32         stroke_is_from_tablet;
} MiltonState;


typedef enum {
    MiltonInputFlags_NONE,
    MiltonInputFlags_FULL_REFRESH    = ( 1 << 0 ),
    MiltonInputFlags_RESET           = ( 1 << 1 ),
    MiltonInputFlags_END_STROKE      = ( 1 << 2 ),
    MiltonInputFlags_UNDO            = ( 1 << 3 ),
    MiltonInputFlags_REDO            = ( 1 << 4 ),
    MiltonInputFlags_SET_MODE_ERASER = ( 1 << 5 ),
    MiltonInputFlags_SET_MODE_BRUSH  = ( 1 << 6 ),
    MiltonInputFlags_FAST_DRAW       = ( 1 << 7 ),
    MiltonInputFlags_HOVERING        = ( 1 << 8 ),
    MiltonInputFlags_PANNING         = ( 1 << 9 ),
} MiltonInputFlags;

typedef struct MiltonInput_s {
    MiltonInputFlags flags;

    v2i  points[MAX_INPUT_BUFFER_ELEMS];
    f32  pressures[MAX_INPUT_BUFFER_ELEMS];
    i32  input_count;

    v2i  hover_point;
    i32  scale;
    v2i  pan_delta;
} MiltonInput;

typedef struct Bitmap_s {
    i32 width;
    i32 height;
    i32 num_components;
    u8* data;
} Bitmap;

// Defined below. Used in rasterizer.h
func i32 milton_get_brush_size(MiltonState* milton_state);

typedef enum {
    MiltonRenderFlags_NONE              = 0,
    MiltonRenderFlags_PICKER_UPDATED    = (1 << 0),
    MiltonRenderFlags_FULL_REDRAW       = (1 << 1),
    MiltonRenderFlags_FINISHED_STROKE   = (1 << 2),
} MiltonRenderFlags;

#include "gui.h"
#include "rasterizer.h"
#include "persist.h"
#include "milton_gl_backend.h"

func void milton_gl_update_brush_hover(MiltonGLState* gl, CanvasView* view, i32 radius)
{
    f32 radiusf = (f32)radius / (f32)view->screen_size.w;
    glUseProgramObjectARB(gl->quad_program);
    GLint loc_radius_sq = glGetUniformLocationARB(gl->quad_program, "radiusf");
    if ( loc_radius_sq >= 0 ) {
        glUniform1fARB(loc_radius_sq, radiusf);
    } else {
        milton_log("[ERROR] Could not set brush overlay in GL\n");
    }
}

func i32 milton_get_brush_size(MiltonState* milton_state)
{
    i32 brush_size = 0;
    if (milton_state->current_mode & MiltonMode_PEN) {
        brush_size = milton_state->brush_sizes[BrushEnum_PEN];
    } else if (milton_state->current_mode & MiltonMode_ERASER) {
        brush_size = milton_state->brush_sizes[BrushEnum_ERASER];
    } else {
        assert (! "milton_get_brush_size called when in invalid mode.");
    }
    return brush_size;
}

func void milton_update_brushes(MiltonState* milton_state)
{
    for (int i = 0; i < BrushEnum_COUNT; ++i ) {
        Brush* brush = &milton_state->brushes[i];
        i32 size = milton_state->brush_sizes[i];
        brush->radius = size * milton_state->view->scale;
        assert(brush->radius < FLT_MAX);
        if (i == BrushEnum_PEN) {
            // Alpha is set by the UI
            brush->color = to_premultiplied(sRGB_to_linear(gui_get_picker_rgb(milton_state->gui)),
                                            brush->alpha);
        } else if (i == BrushEnum_ERASER) {
            brush->color = (v4f) { 1, 1, 1, 1, };
            brush->alpha = 1;
        }
    }
    milton_gl_update_brush_hover(milton_state->gl, milton_state->view,
                                 milton_get_brush_size(milton_state));

    milton_state->working_stroke.brush = milton_state->brushes[BrushEnum_PEN];
}

func Brush milton_get_brush(MiltonState* milton_state)
{
    Brush brush = { 0 };
    if (milton_state->current_mode & MiltonMode_PEN) {
        brush = milton_state->brushes[BrushEnum_PEN];
    } else if (milton_state->current_mode & MiltonMode_ERASER) {
        brush = milton_state->brushes[BrushEnum_ERASER];
    } else {
        assert (!"Picking brush in invalid mode");
    }
    return brush;
}

func i32* pointer_to_brush_size(MiltonState* milton_state)
{
    i32* ptr = NULL;

    if ( milton_state->current_mode & MiltonMode_PEN ) {
        ptr = &milton_state->brush_sizes[BrushEnum_PEN];
    } else if (milton_state->current_mode & MiltonMode_ERASER) {
        ptr = &milton_state->brush_sizes[BrushEnum_ERASER];
    }
    return ptr;
}

func void milton_set_brush_size(MiltonState* milton_state, i32 size)
{
    assert (size < MAX_BRUSH_SIZE);
    (*pointer_to_brush_size(milton_state)) = size;
    milton_update_brushes(milton_state);
}

// For keyboard shortcut.
func void milton_increase_brush_size(MiltonState* milton_state)
{
    i32 brush_size = milton_get_brush_size(milton_state);
    if (brush_size < MAX_BRUSH_SIZE) {
        milton_set_brush_size(milton_state, brush_size + 1);
    }
    milton_update_brushes(milton_state);
}

// For keyboard shortcut.
func void milton_decrease_brush_size(MiltonState* milton_state)
{
    i32 brush_size = milton_get_brush_size(milton_state);

    if (brush_size > 1) {
        milton_set_brush_size(milton_state, brush_size - 1);
    }
    milton_update_brushes(milton_state);
}

func void milton_set_pen_alpha(MiltonState* milton_state, float alpha)
{
    milton_state->brushes[BrushEnum_PEN].alpha = alpha;
    milton_update_brushes(milton_state);
}

#ifndef NDEBUG
func void milton_startup_tests()
{
    v3f rgb = hsv_to_rgb((v3f){ 0 });
    assert(rgb.r == 0 &&
           rgb.g == 0 &&
           rgb.b == 0);
    rgb = hsv_to_rgb((v3f){ 0, 0, 1.0 });
    assert(rgb.r == 1 &&
           rgb.g == 1 &&
           rgb.b == 1);
    rgb = hsv_to_rgb((v3f){ 120, 1.0f, 0.5f });
    assert(rgb.r == 0 &&
           rgb.g == 0.5f &&
           rgb.b == 0);
    rgb = hsv_to_rgb((v3f){ 0, 1.0f, 1.0f });
    assert(rgb.r == 1.0f &&
           rgb.g == 0 &&
           rgb.b == 0);
}

func void milton_blend_tests()
{
    v4f a = { 1,0,0, 0.5f };
    v4f b = { 0,1,0, 0.5f };
    v4f blend = blend_v4f(a, b);
    assert (blend.r > 0);
}

func void milton_math_tests()
{
    v2i a = { 0,  0 };
    v2i b = { 2,  0 };
    v2i u = { 1, -2 };
    v2i v = { 1,  2 };

    v2f intersection;
    b32 hit = intersect_line_segments(a, b,
                                      u, v,
                                      &intersection);
    assert(hit);
    assert(intersection.y == 0);
    assert(intersection.x >= 0.99999 && intersection.x <= 1.00001f);
}

func void milton_cord_tests(Arena* arena)
{
    StrokeCord* cord = StrokeCord_make(arena, 2);
    if ( cord ) {
        for (int i = 0; i < 11; ++i) {
            Stroke test = {0};
            test.num_points = i;
            StrokeCord_push(cord, test);
        }
        for ( int i = 0; i < 11; ++i ) {
            Stroke test = *StrokeCord_get(cord, i);
            assert(test.num_points == i);
        }
    } else {
        assert (!"Could not create cord");
    }
}

func void milton_dynmem_tests()
{
    int* ar1 = dyn_alloc(int, 10);
    int* ar0 = dyn_alloc(int, 1);

    dyn_free(ar1);
    dyn_free(ar0);

    int* ar2 = dyn_alloc(int, 9);
    dyn_free(ar2);
}

#endif

func void milton_load_assets(MiltonState* milton_state)
{
    MiltonGui* gui = milton_state->gui;

    Bitmap* bitmap = &gui->brush_button.bitmap;

    static char* img_name_brush_button = "assets/brush.png";

    bitmap->data = stbi_load(img_name_brush_button, &bitmap->width, &bitmap->height,
                             &bitmap->num_components, 4);
    i32 x = 400;
    i32 y = 500;
    gui->brush_button.rect = rect_from_xywh(x, y, bitmap->width, bitmap->height);
}

func void milton_init(MiltonState* milton_state)
{
    // Initialize dynamic memory stuff.
    {
        MILTON_GLOBAL_dyn_freelist_sentinel = arena_alloc_elem(milton_state->root_arena,
                                                               AllocNode);
        MILTON_GLOBAL_dyn_freelist_sentinel->next = MILTON_GLOBAL_dyn_freelist_sentinel;
        MILTON_GLOBAL_dyn_freelist_sentinel->prev = MILTON_GLOBAL_dyn_freelist_sentinel;
        MILTON_GLOBAL_dyn_root_arena = milton_state->root_arena;
    }
    milton_state->cpu_has_sse2 = SDL_HasSSE2();

    // Initialize render queue
    milton_state->render_queue = arena_alloc_elem(milton_state->root_arena, RenderQueue);
    {
        milton_state->render_queue->work_available      = SDL_CreateSemaphore(0);
        milton_state->render_queue->completed_semaphore = SDL_CreateSemaphore(0);
        milton_state->render_queue->mutex               = SDL_CreateMutex();
    }

    milton_state->num_render_workers = SDL_GetCPUCount();

    milton_log("[DEBUG]: Creating %d render workers.\n", milton_state->num_render_workers);

    milton_state->render_worker_arenas = arena_alloc_array(milton_state->root_arena,
                                                           milton_state->num_render_workers,
                                                           Arena);
    milton_state->worker_memory_size = 65536;

    assert (milton_state->num_render_workers);

#ifndef NDEBUG
    milton_startup_tests();
    milton_blend_tests();
    milton_math_tests();
    milton_cord_tests(milton_state->root_arena);
    milton_dynmem_tests();
#endif

    // Allocate enough memory for the maximum possible supported resolution. As
    // of now, it seems like future 8k displays will adopt this resolution.
    milton_state->bytes_per_pixel = 4;

    milton_state->strokes = StrokeCord_make(milton_state->root_arena, 1024);

    milton_state->working_stroke.points     = arena_alloc_array(milton_state->root_arena,
                                                                STROKE_MAX_POINTS, v2i);
    milton_state->working_stroke.metadata   = arena_alloc_array(milton_state->root_arena,
                                                                STROKE_MAX_POINTS, PointMetadata);

    milton_state->current_mode = MiltonMode_PEN;

    milton_state->gl = arena_alloc_elem(milton_state->root_arena, MiltonGLState);

#if 0
    milton_state->blocks_per_blockgroup = 16;
    milton_state->block_width = 32;
#else
    milton_state->blocks_per_blockgroup = 4;
    milton_state->block_width = 32;
#endif

    // Set the view
    {
        milton_state->view = arena_alloc_elem(milton_state->root_arena, CanvasView);
        milton_state->view->scale = MILTON_DEFAULT_SCALE;
        milton_state->view->downsampling_factor = 1;
        milton_state->view->canvas_radius_limit = 1024 * 1024 * 512;
#if 0
        milton_state->view->rotation = 0;
        for (int d = 0; d < 360; d++)
        {
            f32 r = deegrees_to_radians(d);
            f32 c = cosf(r);
            f32 s = sinf(r);
            milton_state->view->cos_sin_table[d][0] = c;
            milton_state->view->cos_sin_table[d][1] = s;
        }
#endif
    }

    milton_state->gui = arena_alloc_elem(milton_state->root_arena, MiltonGui);
    gui_init(milton_state->root_arena, milton_state->gui);

    milton_gl_backend_init(milton_state);
    milton_load(milton_state);

    // Set default brush sizes.
    for (int i = 0; i < BrushEnum_COUNT; ++i) {
        switch (i) {
        case BrushEnum_PEN:
            milton_state->brush_sizes[i] = 10;
            break;
        case BrushEnum_ERASER:
            milton_state->brush_sizes[i] = 40;
            break;
        default:
            assert(!"New brush has not been given a default size");
            break;
        }
    }

    milton_set_pen_alpha(milton_state, 0.8f);

    for (i32 i = 0; i < milton_state->num_render_workers; ++i) {
        WorkerParams* params = arena_alloc_elem(milton_state->root_arena, WorkerParams);
        {
            *params = (WorkerParams) { milton_state, i };
        }
        assert (milton_state->render_worker_arenas[i].ptr == NULL);
        u8* worker_memory = dyn_alloc(u8, milton_state->worker_memory_size);
        if (!worker_memory) {
            milton_die_gracefully("Platform allocation failed");
        }
        milton_state->render_worker_arenas[i] = arena_init(worker_memory,
                                                           milton_state->worker_memory_size);

        SDL_CreateThread(render_worker, "Milton Render Worker", (void*)params);
    }
    milton_load_assets(milton_state);
}

func b32 is_user_drawing(MiltonState* milton_state)
{
    b32 result = milton_state->working_stroke.num_points > 0;
    return result;
}

func void milton_resize(MiltonState* milton_state, v2i pan_delta, v2i new_screen_size)
{
    if ((new_screen_size.w > 8000 ||
         new_screen_size.h > 8000 ||
         new_screen_size.w <= 0 ||
         new_screen_size.h <= 0) ) {
        return;
    }

    b32 do_realloc = false;
    if (milton_state->max_width <= new_screen_size.w) {
        milton_state->max_width = new_screen_size.w + 256;
        do_realloc = true;
    }
    if (milton_state->max_height <= new_screen_size.h) {
        milton_state->max_height = new_screen_size.h + 256;
        do_realloc = true;
    }

    i32 buffer_size =
            milton_state->max_width * milton_state->max_height * milton_state->bytes_per_pixel;

    if (do_realloc) {
        void* raster_buffer = (void*)milton_state->raster_buffer;
        if (raster_buffer)
        {
            dyn_free(raster_buffer);
        }
        milton_state->raster_buffer = dyn_alloc(u8, buffer_size);

        // TODO: handle this failure gracefully.
        assert(milton_state->raster_buffer);
    }

    if ( new_screen_size.w < milton_state->max_width &&
         new_screen_size.h < milton_state->max_height) {
        milton_state->view->screen_size = new_screen_size;
        milton_state->view->aspect_ratio = (f32)new_screen_size.w / (f32)new_screen_size.h;
        milton_state->view->screen_center = invscale_v2i(milton_state->view->screen_size, 2);

        // Add delta to pan vector
        v2i pan_vector = add_v2i(milton_state->view->pan_vector,
                                 scale_v2i(pan_delta, milton_state->view->scale));

        while (pan_vector.x > milton_state->view->canvas_radius_limit)
        {
            pan_vector.x -= milton_state->view->canvas_radius_limit;
        }
        while (pan_vector.x <= -milton_state->view->canvas_radius_limit)
        {
            pan_vector.x += milton_state->view->canvas_radius_limit;
        }
        while (pan_vector.y > milton_state->view->canvas_radius_limit)
        {
            pan_vector.y -= milton_state->view->canvas_radius_limit;
        }
        while (pan_vector.y <= -milton_state->view->canvas_radius_limit)
        {
            pan_vector.y += milton_state->view->canvas_radius_limit;
        }
        milton_state->view->pan_vector = pan_vector;
    } else {
        assert(!"DEBUG: new screen size is more than we can handle.");
    }
}

func void milton_stroke_input(MiltonState* milton_state, MiltonInput* input)
{
    if ( input->input_count == 0 ) {
        return;
    }

    //milton_log("Stroke input with %d packets\n", input->input_count);
    milton_state->working_stroke.brush = milton_get_brush(milton_state);

    //int input_i = 0;
    for (int input_i = 0; input_i < input->input_count; ++input_i) {
        v2i in_point = input->points[input_i];

        v2i canvas_point = raster_to_canvas(milton_state->view, in_point);

        f32 pressure = NO_PRESSURE_INFO;

        if (input->pressures[input_i] != NO_PRESSURE_INFO)
        {
            f32 pressure_min = 0.20f;
            pressure = pressure_min + input->pressures[input_i] * (1.0f - pressure_min);
            milton_state->stroke_is_from_tablet = true;
        }

        // We haven't received pressure info, assume mouse input
        if (input->pressures[input_i] == NO_PRESSURE_INFO && !milton_state->stroke_is_from_tablet)
        {
            pressure = 1.0f;
        }

        b32 not_the_first = false;
        if ( milton_state->working_stroke.num_points >= 1 ) {
            not_the_first = true;
        }

        // A point passes inspection if:
        //  a) it's the first point of this stroke
        //  b) it is being appended to the stroke and it didn't merge with the previous point.
        b32 passed_inspection = true;

        if ( pressure == NO_PRESSURE_INFO ) {
            passed_inspection = false;
        }

        if (passed_inspection && not_the_first) {
            i32 in_radius = (i32)(pressure * milton_state->working_stroke.brush.radius);

            // Limit the number of points we check so that we don't mess with the strok too much.
            int point_window = 4;
            int count = 0;
            // Pop every point that is contained by the new one.
            for (i32 i = milton_state->working_stroke.num_points - 1; i >= 0; --i) {
                if ( ++count >= point_window ) {
                    break;
                }
                v2i this_point = milton_state->working_stroke.points[i];
                i32 this_radius = (i32)(milton_state->working_stroke.brush.radius *
                                        milton_state->working_stroke.metadata[i].pressure);

                if ( stroke_point_contains_point(canvas_point, in_radius,
                                                this_point, this_radius) ) {
                    milton_state->working_stroke.num_points -= 1;
                } else if ( stroke_point_contains_point(this_point, this_radius,
                                                     canvas_point, in_radius) ) {
                    // If some other point in the past contains this point,
                    // then this point is invalid.
                    passed_inspection = false;
                    break;
                }
            }
        }

        // Cleared to be appended.
        if ( passed_inspection ) {
            // Add to current stroke.
            int index = milton_state->working_stroke.num_points++;
            milton_state->working_stroke.points[index] = canvas_point;


            milton_state->working_stroke.metadata[index] =
                    (PointMetadata) { .pressure = pressure };
        }
    }
}


// Our "render loop" inner function.
func void milton_update(MiltonState* milton_state, MiltonInput* input)
{
    // TODO: Save redo point?
    b32 should_save =
            //(input->point != NULL) ||
            (input->flags & MiltonInputFlags_END_STROKE) ||
            (input->flags & MiltonInputFlags_RESET) ||
            (input->flags & MiltonInputFlags_UNDO) ||
            (input->flags & MiltonInputFlags_REDO);

    arena_reset(milton_state->transient_arena);

    MiltonRenderFlags render_flags = MiltonRenderFlags_NONE;

    b32 do_fast_draw = false;
    if ( milton_state->worker_needs_memory ) {
        i32 prev_memory_value = milton_state->worker_memory_size;
        milton_state->worker_memory_size *= 2;
        i32 needed_size = milton_state->worker_memory_size;

        for (int i = 0; i < milton_state->num_render_workers; ++i)
        {
            if (milton_state->render_worker_arenas[i].ptr != NULL)
            {
                dyn_free(milton_state->render_worker_arenas[i].ptr);
            }
            u8* new_memory = dyn_alloc(u8, needed_size);
            milton_state->render_worker_arenas[i] = arena_init(new_memory, needed_size);
            if (milton_state->render_worker_arenas[i].ptr == NULL)
            {
                milton_die_gracefully("Failed to realloc worker arena\n");
            }
        }

        milton_log("[DEBUG] Assigning more memory per worker. From %d to %d\n",
                   prev_memory_value,
                   milton_state->worker_memory_size);

        milton_state->worker_needs_memory = false;
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
        do_fast_draw = true;
    }

    if ( input->flags & MiltonInputFlags_FAST_DRAW ) {
        do_fast_draw = true;
    }

    if ( do_fast_draw ) {
        milton_state->view->downsampling_factor = 2;
        milton_state->current_mode |= MiltonMode_REQUEST_QUALITY_REDRAW;
    } else {
        milton_state->view->downsampling_factor = 1;
        if ( milton_state->current_mode & MiltonMode_REQUEST_QUALITY_REDRAW ) {
            milton_state->current_mode ^= MiltonMode_REQUEST_QUALITY_REDRAW;
            render_flags |= MiltonRenderFlags_FULL_REDRAW;
        }
    }

    if ( input->flags & MiltonInputFlags_FULL_REFRESH ) {
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
    }

    if ( input->scale ) {
        render_flags |= MiltonRenderFlags_FULL_REDRAW;

// Sensible
#if 1
        f32 scale_factor = 1.3f;
        i32 view_scale_limit = 10000;
// Debug
#else
        f32 scale_factor = 1.5f;
        i32 view_scale_limit = 1000000;
#endif

        static b32 debug_scale_lock = false;
        if ( !debug_scale_lock && input->scale > 0 && milton_state->view->scale >= 2 ) {
            milton_state->view->scale = (i32)(milton_state->view->scale / scale_factor);
            if (milton_state->view->scale == 1)
            {
                debug_scale_lock = true;
            }
        } else if ( input->scale < 0 && milton_state->view->scale < view_scale_limit ) {
            debug_scale_lock = false;
            milton_state->view->scale = (i32)(milton_state->view->scale * scale_factor) + 1;
        }
        milton_update_brushes(milton_state);
    }

    if ( input->flags & MiltonInputFlags_SET_MODE_BRUSH ) {
        milton_state->current_mode = MiltonMode_PEN;
        milton_update_brushes(milton_state);
    }
    if ( input->flags & MiltonInputFlags_SET_MODE_ERASER ) {
        milton_state->current_mode = MiltonMode_ERASER;
        milton_update_brushes(milton_state);
    }

    if ( input->flags & MiltonInputFlags_UNDO ) {
        if ( milton_state->working_stroke.num_points == 0 && milton_state->strokes->count > 0 ) {
            milton_state->strokes->count--;
            milton_state->num_redos++;
        } else if ( milton_state->working_stroke.num_points > 0 ) {
            assert(!"NPE");
        }
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
    } else if ( input->flags & MiltonInputFlags_REDO ) {
        if ( milton_state->num_redos > 0 ) {
            milton_state->strokes->count++;
            milton_state->num_redos--;
        } render_flags |= MiltonRenderFlags_FULL_REDRAW;
    }

    if ( input->flags & MiltonInputFlags_RESET ) {
        milton_state->view->scale = MILTON_DEFAULT_SCALE;
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
        // TODO: Reclaim memory?
        milton_state->strokes->count = 0;
        StrokeCord_get(milton_state->strokes, 0)->num_points = 0;
        milton_state->working_stroke.num_points = 0;
        milton_update_brushes(milton_state);
    }
#if 0
    // ==== Rotate ======
    if (input->rotation != 0)
    {
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
    }
    milton_state->view->rotation += input->rotation;
    while (milton_state->view->rotation < 0)
    {
        milton_state->view->rotation += 360;
    }
    while (milton_state->view->rotation >= 360)
    {
        milton_state->view->rotation -= 360;
    }
#endif


    if ( input->flags & MiltonInputFlags_HOVERING ) {
        milton_state->hover_point = input->hover_point;
        f32 x = input->hover_point.x / (f32)milton_state->view->screen_size.w;
        f32 y = input->hover_point.y / (f32)milton_state->view->screen_size.w;
        milton_gl_set_brush_hover(milton_state->gl, milton_state->view,
                                  milton_get_brush_size(milton_state), x, y);
    }

    if ( input->input_count > 0 ) {
        // Don't draw brush outline.
        milton_gl_unset_brush_hover(milton_state->gl);

        if ( !is_user_drawing(milton_state) &&
             gui_consume_input(milton_state->gui, input) ) {
            milton_update_brushes(milton_state);
            render_flags |= gui_update(milton_state, input);
        } else if (!milton_state->gui->active) {
            milton_stroke_input(milton_state, input);
        }

        // Clear redo stack
        milton_state->num_redos = 0;
    }

    if ( input->flags & MiltonInputFlags_END_STROKE ) {
        milton_state->stroke_is_from_tablet = false;

        if ( milton_state->gui->active ) {
            gui_deactivate(milton_state->gui);
            render_flags |= MiltonRenderFlags_PICKER_UPDATED;
        } else {
            if ( milton_state->working_stroke.num_points > 0 ) {
                // We used the selected color to draw something. Push.
                if(gui_mark_color_used(milton_state->gui,
                                       milton_state->working_stroke.brush.color.rgb)) {
                    render_flags |= MiltonRenderFlags_PICKER_UPDATED;
                }
                // Copy current stroke.
                Stroke new_stroke =
                {
                    .brush = milton_state->working_stroke.brush,
                    .points = arena_alloc_array(milton_state->root_arena,
                                                milton_state->working_stroke.num_points,
                                                v2i),
                    .metadata = arena_alloc_array(milton_state->root_arena,
                                                  milton_state->working_stroke.num_points,
                                                  PointMetadata),
                    .num_points = milton_state->working_stroke.num_points,
                };
                memcpy(new_stroke.points,
                       milton_state->working_stroke.points,
                       milton_state->working_stroke.num_points * sizeof(v2i));
                memcpy(new_stroke.metadata,
                       milton_state->working_stroke.metadata,
                       milton_state->working_stroke.num_points * sizeof(PointMetadata));

                StrokeCord_push(milton_state->strokes, new_stroke);
                // Clear working_stroke
                {
                    milton_state->working_stroke.num_points = 0;
                }
                render_flags |= MiltonRenderFlags_FINISHED_STROKE;
            }
        }
    }

    // Disable hover if panning.
    if ( input->flags & MiltonInputFlags_PANNING ) {
        milton_gl_unset_brush_hover(milton_state->gl);
    }

    milton_render(milton_state, render_flags);

    if ( should_save ) {
        milton_save(milton_state);
    }
}

#ifdef __cplusplus
}
#endif
