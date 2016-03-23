// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.


#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "milton_configuration.h"

#define MILTON_USE_VAO              1
#define STROKE_MAX_POINTS           2048
#define MILTON_DEFAULT_SCALE        (1 << 10)
#define NO_PRESSURE_INFO            -1.0f
#define MAX_INPUT_BUFFER_ELEMS      32
#define MILTON_MINIMUM_SCALE        (1 << 4)
#define QUALITY_REDRAW_TIMEOUT_MS   200
#define MAX_LAYER_NAME_LEN          1024

#define SGL_GL_HELPERS_IMPLEMENTATION
#include "gl_helpers.h"

#include "canvas.h"
#include "color.h"
#include "memory.h"
#include "render_common.h"
#include "utils.h"


typedef struct MiltonGLState
{
    GLuint quad_program;
    GLuint texture;
    GLuint vbo;
#if MILTON_USE_VAO
    GLuint quad_vao;
#endif
} MiltonGLState;

typedef enum MiltonMode
{
    MiltonMode_NONE                   = 0,
    MiltonMode_ERASER                 = 1 << 0,
    MiltonMode_PEN                    = 1 << 1,
    MiltonMode_EXPORTING              = 1 << 2,
} MiltonMode;

enum {
    BrushEnum_PEN,
    BrushEnum_ERASER,

    BrushEnum_COUNT,
};

struct MiltonGui;

typedef struct MiltonState
{
    u8      bytes_per_pixel;

    i32     max_width;
    i32     max_height;
    u8*     raster_buffer;
    u8*     canvas_buffer;

    // The screen is rendered in blockgroups
    // Each blockgroup is rendered in blocks of size (block_width*block_width).
    i32     blocks_per_blockgroup;
    i32     block_width;

    MiltonGLState* gl;

    struct MiltonGui* gui;

    // ---- The Painting
    Brush   brushes[BrushEnum_COUNT];
    i32     brush_sizes[BrushEnum_COUNT];  // In screen pixels

    Layer*  root_layer;
    Layer*  working_layer;
    i32     num_strokes;

    Stroke  working_stroke;
    // ----  // gui->picker.info also stored

    // Stretchy buffer for redo/undo
    Stroke* redo_stack;

    CanvasView* view;
    v2i     hover_point;  // Track the pointer when not stroking..

    // Read only
    // Set these with milton_switch_mode and milton_use_previous_mode
    MiltonMode current_mode;
    MiltonMode last_mode;

    i32 quality_redraw_time;
    b32 request_quality_redraw;  // After drawing with downsampling this gets set to true.

    i32             num_render_workers;
    RenderStack*    render_stack;

    // Heap
    Arena*      root_arena;         // Persistent memory.
    Arena*      render_worker_arenas;

    size_t      worker_memory_size;
    b32         worker_needs_memory;

    // Quick and dirty way to count MOUSE_UP events as stroke points for mouse
    // but discard them when using a tablet.
    b32         stroke_is_from_tablet;

    // This is set to false after it's safe to quit
    b32 running;

    // ====
    // Debug helpers
    // ====
#if MILTON_DEBUG
    b32 DEBUG_sse2_switch;
    b32 DEBUG_replaying;
#endif
} MiltonState;

typedef enum MiltonInputFlags
{
    MiltonInputFlags_NONE = 0,

    MiltonInputFlags_FULL_REFRESH        = 1 << 0,
    MiltonInputFlags_END_STROKE          = 1 << 2,
    MiltonInputFlags_UNDO                = 1 << 3,
    MiltonInputFlags_REDO                = 1 << 4,
    MiltonInputFlags_CHANGE_MODE         = 1 << 5,
    MiltonInputFlags_FAST_DRAW           = 1 << 6,
    MiltonInputFlags_HOVERING            = 1 << 7,
    MiltonInputFlags_PANNING             = 1 << 8,
    MiltonInputFlags_IMGUI_GRABBED_INPUT = 1 << 9,
} MiltonInputFlags;

typedef struct MiltonInput
{
    MiltonInputFlags flags;
    MiltonMode mode_to_set;

    v2i  points[MAX_INPUT_BUFFER_ELEMS];
    f32  pressures[MAX_INPUT_BUFFER_ELEMS];
    i32  input_count;

    v2i  hover_point;
    i32  scale;
    v2i  pan_delta;
} MiltonInput;

// See gl_helpers.h for the reason for defining this
#if defined(__MACH__)
#define glGetAttribLocationARB glGetAttribLocation
#define glGetUniformLocationARB glGetUniformLocation
#define glUseProgramObjectARB glUseProgram
#define glCreateProgramObjectARB glCreateProgram
#define glEnableVertexAttribArrayARB glEnableVertexAttribArray
#define glVertexAttribPointerARB glVertexAttribPointer
#define glGenVertexArrays glGenVertexArraysAPPLE
#define glBindVertexArray glBindVertexArrayAPPLE
#endif

void milton_init(MiltonState* milton_state);

void milton_resize(MiltonState* milton_state, v2i pan_delta, v2i new_screen_size);

void milton_gl_backend_draw(MiltonState* milton_state);

static const i32 k_max_brush_size = 80;

// Between 0 and k_max_brush_size
i32 milton_get_brush_size(MiltonState* milton_state);
void milton_set_brush_size(MiltonState* milton_state, i32 size);
void milton_increase_brush_size(MiltonState* milton_state);
void milton_decrease_brush_size(MiltonState* milton_state);
float milton_get_pen_alpha(MiltonState* milton_state);
void milton_set_pen_alpha(MiltonState* milton_state, float alpha);

void milton_use_previous_mode(MiltonState* milton_state);
void milton_switch_mode(MiltonState* milton_state, MiltonMode mode);

// Our "game loop" inner function.
void milton_update(MiltonState* milton_state, MiltonInput* input);

// If memory has been requested after rendering failed, this function will realloc.
void milton_expand_render_memory(MiltonState* milton_state);

void milton_try_quit(MiltonState* milton_state);

void milton_new_layer(MiltonState* milton_state);
void milton_set_working_layer(MiltonState* milton_state, Layer* layer);

#if defined(__cplusplus)
}
#endif
