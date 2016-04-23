// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


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
    MiltonMode_EYEDROPPER              = 1 << 3,
} MiltonMode;

enum
{
    BrushEnum_PEN,
    BrushEnum_ERASER,

    BrushEnum_COUNT,
};

enum HistoryElementType
{
    HistoryElement_STROKE_ADD,
    //HistoryElement_LAYER_DELETE,
};

typedef struct HistoryElement
{
    int type;
    i32 layer_id;  // HistoryElement_STROKE_ADD
} HistoryElement;

struct MiltonGui;

typedef struct MiltonState
{
    u8  bytes_per_pixel;

    b32 flags;  // See MiltonStateFlags

    i32 max_width;
    i32 max_height;
    u8* raster_buffer; // Final image goes here
    u8* canvas_buffer; // Rasterized canvas stored here

    // The screen is rendered in blockgroups
    // Each blockgroup is rendered in blocks of size (block_width*block_width).
    i32 blocks_per_blockgroup;
    i32 block_width;

    MiltonGLState* gl;

    struct MiltonGui* gui;
    char* mlt_file_path;

    // ---- The Painting
    Brush       brushes[BrushEnum_COUNT];
    i32         brush_sizes[BrushEnum_COUNT];  // In screen pixels

    Layer*      root_layer;
    Layer*      working_layer;

    Stroke      working_stroke;

    CanvasView* view;
    // ----  // gui->picker.info also stored (TODO: store color history buttons)

    // Stretchy buffer for redo/undo
    // - History data  (stretchy buffers)
    HistoryElement* history;
    HistoryElement* redo_stack;
    //Layer**         layer_graveyard;
    Stroke*         stroke_graveyard;


    v2i hover_point;  // Track the pointer when not stroking..

    // Read only
    // Set these with milton_switch_mode and milton_use_previous_mode
    MiltonMode current_mode;
    MiltonMode last_mode;

    i32 quality_redraw_time;

    i32             num_render_workers;
    RenderStack*    render_stack;

    // Heap
    Arena*      root_arena;         // Persistent memory.
    Arena*      render_worker_arenas;

    size_t      worker_memory_size;


    // This is set to false after it's safe to quit

    // ====
    // Debug helpers
    // ====
#if MILTON_DEBUG
    b32 DEBUG_sse2_switch;
    b32 DEBUG_replaying;
#endif
} MiltonState;

enum MiltonStateFlags
{
    MiltonStateFlags_RUNNING                = 1<<0,
    MiltonStateFlags_STROKE_IS_FROM_TABLET  = 1<<1, // Quick and dirty way to count MOUSE_UP events as stroke points for mouse but discard them when using a tablet.
    MiltonStateFlags_REQUEST_QUALITY_REDRAW = 1<<2,
    MiltonStateFlags_WORKER_NEEDS_MEMORY    = 1<<3,
    MiltonStateFlags_NEW_CANVAS             = 1<<4,
    MiltonStateFlags_DEFAULT_CANVAS         = 1<<5,
    MiltonStateFlags_IGNORE_NEXT_CLICKUP    = 1<<6,  // When selecting eyedropper from menu, avoid the click from selecting the color...
};

typedef enum MiltonInputFlags
{
    MiltonInputFlags_NONE = 0,

    MiltonInputFlags_FULL_REFRESH        = 1 << 0,
    MiltonInputFlags_END_STROKE          = 1 << 1,
    MiltonInputFlags_UNDO                = 1 << 2,
    MiltonInputFlags_REDO                = 1 << 3,
    MiltonInputFlags_CHANGE_MODE         = 1 << 4,
    MiltonInputFlags_FAST_DRAW           = 1 << 5,
    MiltonInputFlags_HOVERING            = 1 << 6,
    MiltonInputFlags_PANNING             = 1 << 7,
    MiltonInputFlags_IMGUI_GRABBED_INPUT = 1 << 8,
    MiltonInputFlags_SAVE_FILE           = 1 << 9,
    MiltonInputFlags_OPEN_FILE           = 1 << 10,
    MiltonInputFlags_CLICK               = 1 << 11,
    MiltonInputFlags_CLICKUP             = 1 << 12,
} MiltonInputFlags;
#if defined(__cplusplus)
#define MiltonInputFlags int
#endif

typedef struct MiltonInput
{
    MiltonInputFlags flags;
    MiltonMode mode_to_set;

    v2i  points[MAX_INPUT_BUFFER_ELEMS];
    f32  pressures[MAX_INPUT_BUFFER_ELEMS];
    i32  input_count;

    v2i  click;
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

// Expects absolute path
void milton_set_canvas_file(MiltonState* milton_state, char* fname);
void milton_set_default_canvas_file(MiltonState* milton_state);

void milton_reset_canvas(MiltonState* milton_state);

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
void milton_delete_working_layer(MiltonState* milton_state);

#if defined(__cplusplus)
}
#endif
