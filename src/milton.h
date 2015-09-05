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


#if defined(_WIN32)
#include "platform_windows.h"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.h"
#endif

#define MILTON_USE_VAO          1
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



typedef struct MiltonGLState_s
{
    GLuint quad_program;
    GLuint texture;
    GLuint vbo;
#if MILTON_USE_VAO
    GLuint quad_vao;
#endif
} MiltonGLState;

typedef enum MiltonMode_s
{
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
typedef struct BlockgroupRenderData_s
{
    i32     block_start;
} BlockgroupRenderData;

typedef struct RenderQueue_s
{
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

enum
{
    BrushEnum_PEN,
    BrushEnum_ERASER,

    BrushEnum_COUNT,
};

typedef struct MiltonState_s
{
    u8      bytes_per_pixel;

    i32     max_width;
    i32     max_height;
    u8*     raster_buffer;

    // The screen is rendered in blockgroups
    // Each blockgroup is rendered in blocks of size (block_width*block_width).
    i32     blocks_per_blockgroup;
    i32     block_width;

    MiltonGLState* gl;

    ColorPicker picker;

    Brush   brushes[BrushEnum_COUNT];
    i32     brush_sizes[BrushEnum_COUNT];  // In screen pixels

    b32 is_ui_active;  // When interacting with the UI.

    CanvasView* view;
    v2i     hover_point;  // Track the pointer when not stroking..

    Stroke  working_stroke;

    StrokeDeque* strokes;

    i32     num_redos;

    MiltonMode current_mode;

    i32             num_render_workers;
    RenderQueue*    render_queue;


    // Heap
    Arena*      root_arena;         // Persistent memory.
    Arena*      transient_arena;    // Gets reset after every call to milton_update().
    Arena*      render_worker_arenas;

    size_t      worker_memory_size;
    b32         worker_needs_memory;

    b32         cpu_has_sse2;
    b32         stroke_is_from_tablet;
} MiltonState;

typedef enum
{
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

typedef struct MiltonInput_s
{
    MiltonInputFlags flags;

    v2i  points[MAX_INPUT_BUFFER_ELEMS];
    f32  pressures[MAX_INPUT_BUFFER_ELEMS];
    i32  input_count;

    v2i  hover_point;
    i32  scale;
    v2i  pan_delta;
} MiltonInput;


// Defined below. Used in rasterizer.h
func i32 milton_get_brush_size(MiltonState* milton_state);

#include "rasterizer.h"
#include "persist.h"

func void milton_gl_backend_draw(MiltonState* milton_state)
{
    MiltonGLState* gl = milton_state->gl;
    u8* raster_buffer = milton_state->raster_buffer;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)raster_buffer);

    GLCHK (glUseProgram(gl->quad_program));
#if MILTON_USE_VAO
    glBindVertexArray(gl->quad_vao);
#else
    GLint pos_loc     = glGetAttribLocation(gl->quad_program, "position");
    GLint sampler_loc = glGetUniformLocation(gl->quad_program, "raster_buffer");
    assert (pos_loc     >= 0);
    assert (sampler_loc >= 0);
    GLCHK (glUniform1i(sampler_loc, 0 /*GL_TEXTURE0*/));
    GLCHK (glVertexAttribPointer(/*attrib location*/pos_loc,
                                 /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                 /*stride*/0, /*ptr*/0));

    GLCHK (glEnableVertexAttribArray(pos_loc));
#endif
    GLCHK (glDrawArrays (GL_TRIANGLE_FAN, 0, 4) );
}

func void milton_fatal(char* message)
{
    milton_log("*** [FATAL] ***: \n\t");
    puts(message);
    exit(EXIT_FAILURE);
}

func void milton_gl_update_brush_hover(MiltonGLState* gl, CanvasView* view, i32 radius)
{
    f32 radiusf = (f32)radius / (f32)view->screen_size.w;
    glUseProgram(gl->quad_program);
    GLint loc_radius_sq = glGetUniformLocation(gl->quad_program, "radiusf");
    assert ( loc_radius_sq >= 0 );
    glUniform1f(loc_radius_sq, radiusf);
}

func void milton_gl_set_brush_hover(MiltonGLState* gl,
                                    CanvasView* view,
                                    int radius,
                                    f32 x, f32 y)
{
    glUseProgram(gl->quad_program);

    i32 width = view->screen_size.w;

    f32 radiusf = (f32)radius / (f32)width;

    GLint loc_hover_on  = glGetUniformLocation(gl->quad_program, "hover_on");
    GLint loc_radius_sq = glGetUniformLocation(gl->quad_program, "radiusf");
    GLint loc_pointer   = glGetUniformLocation(gl->quad_program, "pointer");
    GLint loc_aspect    = glGetUniformLocation(gl->quad_program, "aspect_ratio");
    assert ( loc_hover_on >= 0 );
    assert ( loc_radius_sq >= 0 );
    assert ( loc_pointer >= 0 );
    assert ( loc_aspect >= 0 );

    glUniform1i(loc_hover_on, 1);
    glUniform1f(loc_radius_sq, radiusf);
    glUniform2f(loc_pointer, x, y);
    glUniform1f(loc_aspect, view->aspect_ratio);
}

func void milton_gl_unset_brush_hover(MiltonGLState* gl)
{
    glUseProgram(gl->quad_program);

    GLint loc_hover_on = glGetUniformLocation(gl->quad_program, "hover_on");
    assert ( loc_hover_on >= 0 );

    glUniform1i(loc_hover_on, 0);
}

func void milton_gl_backend_init(MiltonState* milton_state)
{
    // Init quad program
    {
        const char* shader_contents[2];
        shader_contents[0] =
            "#version 150\n"
            "in vec2 position;\n"
            "\n"
            "out vec2 coord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "   coord = (position + vec2(1.0,1.0))/2.0;\n"
            "   coord.y = 1.0 - coord.y;"
            "   // direct to clip space. must be in [-1, 1]^2\n"
            "   gl_Position = vec4(position, 0.0, 1.0);\n"
            "}\n";

        shader_contents[1] =
            "#version 150\n"
            "\n"
            "uniform sampler2D raster_buffer;\n"
            "uniform bool hover_on;\n"
            "uniform vec2 pointer;\n"
            "uniform float radiusf;\n"
            "uniform float aspect_ratio;\n"
            "in vec2 coord;\n"
            "out vec4 out_color;\n"
            "\n"
            "void main(void)\n"
            "{\n"
            "   vec4 color = texture(raster_buffer, coord).bgra; \n"
            "   if (hover_on)\n"
            "   { \n"
            "       float girth = 0.001; \n"
            "       float dx = (coord.x - pointer.x); \n"
            "       float dy = (coord.y / aspect_ratio - pointer.y); \n"
            "       float dist = sqrt(dx * dx + dy * dy); \n"
            "       bool test1 = ( dist < radiusf + girth ); \n"
            "       bool test2 = ( dist > radiusf - girth ); \n"
            "       float t = float(test1 && test2); \n"
            "       color = t * vec4(0.5,0.5,0.5,1) + (1 - t) * color; \n"
            "   } \n"
            //"   gl_FragColor = color; \n"
            "   out_color = color; \n"
            "}\n";

        GLuint shader_objects[2] = {0};
        for ( int i = 0; i < 2; ++i )
        {
            GLuint shader_type = (i == 0) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
            shader_objects[i] = gl_compile_shader(shader_contents[i], shader_type);
        }
        milton_state->gl->quad_program = glCreateProgram();
        gl_link_program(milton_state->gl->quad_program, shader_objects, 2);

        GLCHK (glUseProgram(milton_state->gl->quad_program));
    }

    // Create texture
    {
         GLCHK (glActiveTexture (GL_TEXTURE0) );
        // Create texture
        GLCHK (glGenTextures   (1, &milton_state->gl->texture));
        GLCHK (glBindTexture   (GL_TEXTURE_2D, milton_state->gl->texture));

        // Note for the future: These are needed.
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    }
    // Create quad
    {
#define u -1.0f
        // full
        GLfloat vert_data[] =
        {
            -u, +u,
            -u, -u,
            +u, -u,
            +u, +u,
        };
#undef u
#if MILTON_USE_VAO
        GLCHK (glGenVertexArrays(1, &milton_state->gl->quad_vao));
        GLCHK (glBindVertexArray(milton_state->gl->quad_vao));
#endif

        GLCHK (glGenBuffers(1, &milton_state->gl->vbo));
        assert (milton_state->gl->vbo > 0);
        GLCHK (glBindBuffer(GL_ARRAY_BUFFER, milton_state->gl->vbo));

        GLint pos_loc     = glGetAttribLocation(milton_state->gl->quad_program, "position");
        GLint sampler_loc = glGetUniformLocation(milton_state->gl->quad_program, "raster_buffer");
        assert (pos_loc     >= 0);
        assert (sampler_loc >= 0);
        GLCHK (glBufferData (GL_ARRAY_BUFFER, sizeof(vert_data), vert_data, GL_STATIC_DRAW));
#if MILTON_USE_VAO
        GLCHK (glVertexAttribPointer     (/*attrib location*/pos_loc,
                    /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE, /*stride*/0, /*ptr*/0));
        GLCHK (glEnableVertexAttribArray (pos_loc) );
#endif
    }
}

func i32 milton_get_brush_size(MiltonState* milton_state)
{
    i32 brush_size = 0;
    if (milton_state->current_mode & MiltonMode_PEN)
    {
        brush_size = milton_state->brush_sizes[BrushEnum_PEN];
    }
    else if (milton_state->current_mode & MiltonMode_ERASER)
    {
        brush_size = milton_state->brush_sizes[BrushEnum_ERASER];
    }
    else
    {
        assert (! "milton_get_brush_size called when in invalid mode.");
    }
    return brush_size;
}

func void milton_update_brushes(MiltonState* milton_state)
{
    for (int i = 0; i < BrushEnum_COUNT; ++i )
    {
        Brush* brush = &milton_state->brushes[i];
        i32 size = milton_state->brush_sizes[i];
        brush->radius = size * milton_state->view->scale;
        assert(brush->radius < FLT_MAX);
        if (i == BrushEnum_PEN)
        {
            // Alpha is set by the UI
            brush->color = to_premultiplied(sRGB_to_linear(hsv_to_rgb(milton_state->picker.hsv)),
                                            brush->alpha);
        }
        else if (i == BrushEnum_ERASER)
        {
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
    if (milton_state->current_mode & MiltonMode_PEN)
    {
        brush = milton_state->brushes[BrushEnum_PEN];
    }
    else if (milton_state->current_mode & MiltonMode_ERASER)
    {
        brush = milton_state->brushes[BrushEnum_ERASER];
    }
    else
    {
        assert (!"Picking brush in invalid mode");
    }
    return brush;
}

func i32* pointer_to_brush_size(MiltonState* milton_state)
{
    i32* ptr = NULL;

    if (milton_state->current_mode & MiltonMode_PEN)
    {
        ptr = &milton_state->brush_sizes[BrushEnum_PEN];
    }
    else if (milton_state->current_mode & MiltonMode_ERASER)
    {
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
    if (brush_size < MAX_BRUSH_SIZE)
    {
        milton_set_brush_size(milton_state, brush_size + 1);
    }
    milton_update_brushes(milton_state);
}

// For keyboard shortcut.
func void milton_decrease_brush_size(MiltonState* milton_state)
{
    i32 brush_size = milton_get_brush_size(milton_state);

    if (brush_size > 1)
    {
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
    v2i a = { 0,    0  };
    v2i b = { 2,    0  };
    v2i u = { 1, -2 };
    v2i v = { 1, 2  };

    v2f intersection;
    b32 hit = intersect_line_segments(a, b,
                                      u, v,
                                      &intersection);
    assert(hit);
    assert(intersection.y == 0);
    assert(intersection.x >= 0.99999 && intersection.x <= 1.00001f);
}

func void milton_deque_tests(Arena* arena)
{
    StrokeDeque* deque = StrokeDeque_make(arena, 2);
    if (deque)
    {
        for (int i = 0; i < 11; ++i)
        {
            Stroke test = {0};
            test.num_points = i;
            StrokeDeque_push(deque, test);
        }
        for (int i = 0; i < 11; ++i)
        {
            Stroke test = *StrokeDeque_get(deque, i);
            assert(test.num_points == i);
        }
    }

}
#endif

func void milton_init(MiltonState* milton_state)
{
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
    milton_deque_tests(milton_state->root_arena);
#endif
    // Allocate enough memory for the maximum possible supported resolution. As
    // of now, it seems like future 8k displays will adopt this resolution.
    milton_state->bytes_per_pixel  = 4;

    milton_state->strokes = StrokeDeque_make(milton_state->root_arena, 1024);

    milton_state->working_stroke.points     = arena_alloc_array(milton_state->root_arena,
                                                                STROKE_MAX_POINTS, v2i);
    milton_state->working_stroke.metadata   = arena_alloc_array(milton_state->root_arena,
                                                                STROKE_MAX_POINTS, PointMetadata);

    milton_state->current_mode = MiltonMode_PEN;

    milton_state->gl = arena_alloc_elem(milton_state->root_arena, MiltonGLState);

#if 1
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

    // Init picker
    {
        i32 bounds_radius_px = 100;
        f32 wheel_half_width = 12;
        milton_state->picker.center = (v2i){ bounds_radius_px + 20, bounds_radius_px + 20 };
        milton_state->picker.bounds_radius_px = bounds_radius_px;
        milton_state->picker.wheel_half_width = wheel_half_width;
        milton_state->picker.wheel_radius = (f32)bounds_radius_px - 5.0f - wheel_half_width;
        milton_state->picker.hsv = (v3f){ 0.0f, 1.0f, 0.7f };
        Rect bounds;
        bounds.left = milton_state->picker.center.x - bounds_radius_px;
        bounds.right = milton_state->picker.center.x + bounds_radius_px;
        bounds.top = milton_state->picker.center.y - bounds_radius_px;
        bounds.bottom = milton_state->picker.center.y + bounds_radius_px;
        milton_state->picker.bounds = bounds;
        milton_state->picker.pixels = arena_alloc_array(milton_state->root_arena,
                                                        (4 * bounds_radius_px * bounds_radius_px),
                                                        u32);
        picker_init(&milton_state->picker);
    }


    milton_gl_backend_init(milton_state);
    milton_load(milton_state);

    // Set default brush sizes.
    for (int i = 0; i < BrushEnum_COUNT; ++i)
    {
        switch (i)
        {
        case BrushEnum_PEN:
            {
                milton_state->brush_sizes[i] = 10;
                break;
            }
        case BrushEnum_ERASER:
            {
                milton_state->brush_sizes[i] = 40;
                break;
            }
        default:
            {
                assert(!"New brush has not been given a default size");
                break;
            }
        }
    }

    milton_set_pen_alpha(milton_state, 0.8f);
    milton_update_brushes(milton_state);

    for (i32 i = 0; i < milton_state->num_render_workers; ++i)
    {
        WorkerParams* params = arena_alloc_elem(milton_state->root_arena, WorkerParams);
        {
            *params = (WorkerParams) { milton_state, i };
        }

        SDL_CreateThread(render_worker, "Milton Render Worker", (void*)params);
    }
}

func b32 is_user_drawing(MiltonState* milton_state)
{
    b32 result = milton_state->working_stroke.num_points > 0;
    return result;
}

func void milton_resize(MiltonState* milton_state, v2i pan_delta, v2i new_screen_size)
{
    if (new_screen_size.w > 8000 ||
        new_screen_size.h > 8000 ||
        new_screen_size.w <= 0 ||
        new_screen_size.h <= 0 )
    {
        return;
    }

    b32 do_realloc = false;
    if (milton_state->max_width <= new_screen_size.w)
    {
        milton_state->max_width = new_screen_size.w + 256;
        do_realloc = true;
    }
    if (milton_state->max_height <= new_screen_size.h)
    {
        milton_state->max_height = new_screen_size.h + 256;
        do_realloc = true;
    }

    size_t buffer_size =
            milton_state->max_width * milton_state->max_height * milton_state->bytes_per_pixel;

    if (do_realloc)
    {
        void* raster_buffer = (void*)milton_state->raster_buffer;
        if (raster_buffer)
        {
            free(raster_buffer);
        }
        milton_state->raster_buffer = (u8*)malloc( buffer_size );

        // TODO: handle this failure gracefully.
        assert(milton_state->raster_buffer);
    }

    if (new_screen_size.w < milton_state->max_width &&
        new_screen_size.h < milton_state->max_height)
    {

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
    }
    else
    {
        assert(!"DEBUG: new screen size is more than we can handle.");
    }
}

func void milton_stroke_input(MiltonState* milton_state, MiltonInput* input)
{
    if (input->input_count == 0)
    {
        return;
    }

    //milton_log("Stroke input with %d packets\n", input->input_count);
    milton_state->working_stroke.brush = milton_get_brush(milton_state);

    //int input_i = 0;
    for (int input_i = 0; input_i < input->input_count; ++input_i)
    {
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
        if (milton_state->working_stroke.num_points >= 1)
        {
            not_the_first = true;
        }

        // A point passes inspection if:
        //  a) it's the first point of this stroke
        //  b) it is being appended to the stroke and it didn't merge with the previous point.
        b32 passed_inspection = true;

        if (pressure == NO_PRESSURE_INFO)
        {
            passed_inspection = false;
        }


        if (passed_inspection && not_the_first)
        {
            i32 in_radius = (i32)(pressure * milton_state->working_stroke.brush.radius);

            // Limit the number of points we check so that we don't mess with the strok too much.
            int point_window = 4;
            int count = 0;
            // Pop every point that is contained by the new one.
            for (i32 i = milton_state->working_stroke.num_points - 1; i >= 0; --i)
            {
                if (++count >= point_window)
                {
                    break;
                }
                v2i this_point = milton_state->working_stroke.points[i];
                i32 this_radius = (i32)(milton_state->working_stroke.brush.radius *
                                        milton_state->working_stroke.metadata[i].pressure);

                if (stroke_point_contains_point(canvas_point, in_radius,
                                                this_point, this_radius))
                {
                    milton_state->working_stroke.num_points -= 1;
                }
                // If some other point in the past contains this point,
                // then this point is invalid.
                else if (stroke_point_contains_point(this_point, this_radius,
                                                     canvas_point, in_radius))
                {
                    passed_inspection = false;
                    break;
                }
            }
        }

        // Cleared to be appended.
        if (passed_inspection)
        {
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
    if (milton_state->worker_needs_memory)
    {
        size_t render_memory_cap = milton_state->worker_memory_size;

        for (int i = 0; i < milton_state->num_render_workers; ++i)
        {
            if (milton_state->render_worker_arenas[i].ptr != NULL)
            {
                platform_deallocate(milton_state->render_worker_arenas[i].ptr);
            }
            milton_state->render_worker_arenas[i] =
                    arena_init(platform_allocate(render_memory_cap),
                               render_memory_cap);
            if (milton_state->render_worker_arenas[i].ptr == NULL)
            {
                milton_fatal("Failed to realloc worker arena\n");
            }
        }

        milton_log("[DEBUG] Assigning more memory per worker. From %li to %li\n",
                   milton_state->worker_memory_size,
                   milton_state->worker_memory_size * 2);

        milton_state->worker_memory_size *= 2;
        milton_state->worker_needs_memory = false;
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
        do_fast_draw = true;
    }

    if (input->flags & MiltonInputFlags_FAST_DRAW)
    {
        do_fast_draw = true;
    }

    if (do_fast_draw)
    {
        milton_state->view->downsampling_factor = 2;
        milton_state->current_mode |= MiltonMode_REQUEST_QUALITY_REDRAW;
    }
    else
    {
        milton_state->view->downsampling_factor = 1;
        if (milton_state->current_mode & MiltonMode_REQUEST_QUALITY_REDRAW)
        {
            milton_state->current_mode ^= MiltonMode_REQUEST_QUALITY_REDRAW;
            render_flags |= MiltonRenderFlags_FULL_REDRAW;
        }
    }

    if (input->flags & MiltonInputFlags_FULL_REFRESH)
    {
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
    }

    if (input->scale)
    {
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
        if (!debug_scale_lock && input->scale > 0 && milton_state->view->scale >= 2)
        {
            milton_state->view->scale = (i32)(milton_state->view->scale / scale_factor);
            if (milton_state->view->scale == 1)
            {
                debug_scale_lock = true;
            }
        }
        else if (input->scale < 0 && milton_state->view->scale < view_scale_limit)
        {
            debug_scale_lock = false;
            milton_state->view->scale = (i32)(milton_state->view->scale * scale_factor) + 1;
        }
        milton_update_brushes(milton_state);
    }

    if (input->flags & MiltonInputFlags_SET_MODE_BRUSH)
    {
        milton_state->current_mode = MiltonMode_PEN;
        milton_update_brushes(milton_state);
    }
    if (input->flags & MiltonInputFlags_SET_MODE_ERASER)
    {
        milton_state->current_mode = MiltonMode_ERASER;
        milton_update_brushes(milton_state);
    }

    if (input->flags & MiltonInputFlags_UNDO)
    {
        if (milton_state->working_stroke.num_points == 0 && milton_state->strokes->count > 0)
        {
            milton_state->strokes->count--;
            milton_state->num_redos++;
        }
        else if (milton_state->working_stroke.num_points > 0)
        {
            assert(!"NPE");
        }
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
    }
    else if (input->flags & MiltonInputFlags_REDO)
    {
        if (milton_state->num_redos > 0)
        {
            milton_state->strokes->count++;
            milton_state->num_redos--;
        }
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
    }

    if (input->flags & MiltonInputFlags_RESET)
    {
        milton_state->view->scale = MILTON_DEFAULT_SCALE;
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
        // TODO: Reclaim memory?
        milton_state->strokes->count = 0;
        StrokeDeque_get(milton_state->strokes, 0)->num_points = 0;
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


    render_flags |= MiltonRenderFlags_BRUSH_OVERLAY;
    if (input->flags & MiltonInputFlags_HOVERING)
    {
        milton_state->hover_point = input->hover_point;
        f32 x = input->hover_point.x / (f32)milton_state->view->screen_size.w;
        f32 y = input->hover_point.y / (f32)milton_state->view->screen_size.w;
        milton_gl_set_brush_hover(milton_state->gl, milton_state->view,
                                  milton_get_brush_size(milton_state), x, y);
    }

    if (input->input_count > 0)
    {
        // Don't draw brush outline.
        milton_gl_unset_brush_hover(milton_state->gl);

        render_flags ^= MiltonRenderFlags_BRUSH_OVERLAY;
        v2i point = input->points[0];
        if (!is_user_drawing(milton_state) &&
            is_picker_accepting_input(&milton_state->picker, point))
        {
            {
                ColorPickResult pick_result = picker_update(&milton_state->picker, point);
                if ((pick_result & ColorPickResult_CHANGE_COLOR) &&
                    (milton_state->current_mode == MiltonMode_PEN))
                {
                    v3f rgb = hsv_to_rgb(milton_state->picker.hsv);
                    milton_state->brushes[BrushEnum_PEN].color =
                            to_premultiplied(rgb, milton_state->brushes[BrushEnum_PEN].alpha);
                }
                render_flags |= MiltonRenderFlags_PICKER_UPDATED;
                milton_state->is_ui_active = true;
            }
        }
        // Currently drawing
        else if (!milton_state->is_ui_active)
        {
            milton_stroke_input(milton_state, input);
        }

        // Clear redo stack
        milton_state->num_redos = 0;
    }

    if (input->flags & MiltonInputFlags_END_STROKE)
    {
        milton_state->stroke_is_from_tablet = false;

        if (milton_state->is_ui_active)
        {
            picker_deactivate(&milton_state->picker);
            milton_state->is_ui_active = false;
            render_flags |= MiltonRenderFlags_PICKER_UPDATED;
        }
        else
        {
            if (milton_state->working_stroke.num_points > 0)
            {
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

                StrokeDeque_push(milton_state->strokes, new_stroke);
                // Clear working_stroke
                {
                    milton_state->working_stroke.num_points = 0;
                }
                render_flags |= MiltonRenderFlags_FINISHED_STROKE;
            }
        }
    }

    // Disable hover if panning.
    if (input->flags & MiltonInputFlags_PANNING)
    {
        milton_gl_unset_brush_hover(milton_state->gl);
    }

    milton_render(milton_state, render_flags);

    if (should_save)
    {
        milton_save(milton_state);
    }
}

#ifdef __cplusplus
}
#endif
