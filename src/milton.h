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


#pragma once

#include "define_types.h"
#include "milton_configuration.h"

#define MILTON_USE_VAO          0
#define RENDER_QUEUE_SIZE       (1 << 13)
#define STROKE_MAX_POINTS       2048
#define MAX_BRUSH_SIZE          80
#define MILTON_DEFAULT_SCALE    (1 << 10)
#define NO_PRESSURE_INFO        -1.0f
#define MAX_INPUT_BUFFER_ELEMS  32

#define SGL_GL_HELPERS_IMPLEMENTATION
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

// TODO: smells move 'request' elsewhere and make this not be a flag
enum class MiltonMode {
    none                   = 0,

    ERASER                 = 1 << 0,
    PEN                    = 1 << 1,
    REQUEST_QUALITY_REDRAW = 1 << 2,
};
DECLARE_FLAG(MiltonMode)

enum class MiltonRenderFlags {
    NONE            = 0,

    PICKER_UPDATED  = 1 << 0,
    FULL_REDRAW     = 1 << 1,
    FINISHED_STROKE = 1 << 2,
    PAN_COPY        = 1 << 3,
};
DECLARE_FLAG(MiltonRenderFlags);

// Render Workers:
//    We have a bunch of workers running on threads, who wait on a lockless
//    queue to take BlockgroupRenderData structures.
//    When there is work available, they call blockgroup_render_thread with the
//    appropriate parameters.
struct BlockgroupRenderData {
    i32     block_start;
};

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

#ifndef NDEBUG
typedef enum {
    DEBUG_BACKEND_CHOICE_avx,
    DEBUG_BACKEND_CHOICE_sse2,
    DEBUG_BACKEND_CHOICE_scalar,

    DEBUG_BACKEND_CHOICE_count,
} MILTONDEBUG_BackendChoice;


static char* MILTONDEBUG_BackendChoiceStrings[DEBUG_BACKEND_CHOICE_count] =
{
    "DEBUG_BACKEND_CHOICE_avx",
    "DEBUG_BACKEND_CHOICE_sse2",
    "DEBUG_BACKEND_CHOICE_scalar",
};

#endif

struct MiltonGui;  // Fowrard decl.

struct MiltonState {
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

    StrokeCord strokes;

    i32     num_redos;

    MiltonMode current_mode;  // TODO: `milton_mode` is not clear. Change approach before adding functionality.

    i32             num_render_workers;
    RenderQueue*    render_queue;


    // Heap
    Arena*      root_arena;         // Persistent memory.
    Arena*      render_worker_arenas;

    size_t      worker_memory_size;
    b32         worker_needs_memory;

    CPUCaps     cpu_caps;
    b32         stroke_is_from_tablet;

    // ====
    // Debug helpers
    // ====
#ifndef NDEBUG
    MILTONDEBUG_BackendChoice DEBUG_backend_choice;
#endif
};


enum class MiltonInputFlags {
    NONE = 0,
    FULL_REFRESH    = 1 << 0,
    RESET           = 1 << 1,
    END_STROKE      = 1 << 2,
    UNDO            = 1 << 3,
    REDO            = 1 << 4,
    SET_MODE_ERASER = 1 << 5,
    SET_MODE_BRUSH  = 1 << 6,
    FAST_DRAW       = 1 << 7,
    HOVERING        = 1 << 8,
    PANNING         = 1 << 9,
};
DECLARE_FLAG(MiltonInputFlags);

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
static i32 milton_get_brush_size(MiltonState* milton_state);

#include "gui.h"
#include "rasterizer.h"
#include "persist.h"

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
void milton_set_brush_size(MiltonState* milton_state, i32 size);

// For keyboard shortcut.
void milton_increase_brush_size(MiltonState* milton_state);

// For keyboard shortcut.
void milton_decrease_brush_size(MiltonState* milton_state);

void milton_set_pen_alpha(MiltonState* milton_state, float alpha);

// Our "game loop" inner function.
void milton_update(MiltonState* milton_state, MiltonInput* input);

