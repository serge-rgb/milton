// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once


#define MILTON_USE_VAO              0
#define STROKE_MAX_POINTS           2048
#define MILTON_DEFAULT_SCALE        (1 << 10)
#define NO_PRESSURE_INFO            -1.0f
#define MAX_INPUT_BUFFER_ELEMS      32
#define MILTON_MINIMUM_SCALE        (1 << 4)
#define QUALITY_REDRAW_TIMEOUT_MS   200
#define MAX_LAYER_NAME_LEN          64
#define MILTON_MAX_BRUSH_SIZE       80
#define MILTON_HIDE_BRUSH_OVERLAY_AT_THIS_SIZE 12
#define HOVER_FLASH_THRESHOLD_MS    500  // How long does the hidden brush hover show when it has changed size.


struct MiltonGLState
{
    GLuint quad_program;
    GLuint texture;
    GLuint vbo;
#if MILTON_USE_VAO
    GLuint quad_vao;
#endif
};

enum MiltonMode
{
    MiltonMode_NONE       = 0,
    MiltonMode_ERASER     = 1 << 0,
    MiltonMode_PEN        = 1 << 1,
    MiltonMode_EXPORTING  = 1 << 2,
    MiltonMode_EYEDROPPER = 1 << 3,
};

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

struct HistoryElement
{
    int type;
    i32 layer_id;  // HistoryElement_STROKE_ADD
};

struct MiltonGui;
struct RenderData;

struct MiltonState
{
    u8  bytes_per_pixel;

    b32 flags;  // See MiltonStateFlags

    i32 max_width;
    i32 max_height;
    u8* raster_buffer; // Final image goes here
    u8* canvas_buffer; // Rasterized canvas stored here
    u8* eyedropper_buffer;  // Get pixels from OpenGL framebuffer and store them here for eydropper operations.

    // The screen is rendered in blockgroups
    // Each blockgroup is rendered in blocks of size (block_width*block_width).
    i32 blocks_per_blockgroup;
    i32 block_width;

    MiltonGLState* gl;

    struct MiltonGui* gui;

    // Persistence
    PATH_CHAR*  mlt_file_path;
    u32         mlt_binary_version;
    WallTime    last_save_time;
    i64         last_save_stroke_count;  // This is a workaround to MoveFileEx failing occasionally, particularaly when
                                        // when the mlt file gets large.
                                        // Check that all the strokes are saved at quit time in case that
                                        // the last MoveFileEx failed.
#if MILTON_SAVE_ASYNC
    SDL_mutex*  save_mutex;
    i64         save_flag;   // See SaveEnum
#endif

    // ---- The Painting
    Brush       brushes[BrushEnum_COUNT];
    i32         brush_sizes[BrushEnum_COUNT];  // In screen pixels

    i32         layer_guid;  // to create unique ids;
    Layer*      root_layer;
    Layer*      working_layer;

    Stroke      working_stroke;

    CanvasView* view;
    i32 real_scale;  // Like view->scale but dependent on supersampling scale. i.e. view->scale/SSAA_FACTOR

    // ----  // gui->picker.info also stored

    DArray<HistoryElement> history;
    DArray<HistoryElement> redo_stack;
    //Layer**         layer_graveyard;
    DArray<Stroke>         stroke_graveyard;


    v2i hover_point;  // Track the pointer when not stroking..
    i32 hover_flash_ms;  // Set on keyboard shortcut to change brush size.
                        // Brush hover "flashes" if it is currently hidden to show its current size.


    // Read only
    // Set these with milton_switch_mode and milton_use_previous_mode
    MiltonMode current_mode;
    MiltonMode last_mode;

    i32 quality_redraw_time;

    RenderData* render_data;  // Hardware Renderer

    i32             num_render_workers;
    RenderStack*    render_stack;

    // Heap
    Arena*      root_arena;         // Bounded allocations
    Arena*      render_worker_arenas;

    size_t      worker_memory_size;

    // ====
    // Debug helpers
    // ====
#if MILTON_DEBUG
    b32 DEBUG_sse2_switch;
    b32 DEBUG_replaying;
#endif
#if MILTON_ENABLE_PROFILING
    b32 viz_window_visible;
    GraphData graph_frame;
#endif
};

enum MiltonStateFlags
{
    MiltonStateFlags_RUNNING                = 1 << 0,
    MiltonStateFlags_STROKE_IS_FROM_TABLET  = 1 << 1, // Quick and dirty way to count MOUSE_UP events as stroke points for mouse but discard them when using a tablet.
    MiltonStateFlags_REQUEST_QUALITY_REDRAW = 1 << 2,
    MiltonStateFlags_WORKER_NEEDS_MEMORY    = 1 << 3,
    MiltonStateFlags_NEW_CANVAS             = 1 << 4,
    MiltonStateFlags_DEFAULT_CANVAS         = 1 << 5,
    MiltonStateFlags_IGNORE_NEXT_CLICKUP    = 1 << 6,  // When selecting eyedropper from menu, avoid the click from selecting the color...
    MiltonStateFlags_BRUSH_SIZE_CHANGED     = 1 << 7,
    MiltonStateFlags_BRUSH_HOVER_FLASHING   = 1 << 8,  // Send a GUI redraw event on timeout if overlay is hidden.
    MiltonStateFlags_LAST_SAVE_FAILED       = 1 << 9,
    MiltonStateFlags_MOVE_FILE_FAILED       = 1 << 10,
    MiltonStateFlags_BRUSH_SMOOTHING        = 1 << 11,
};

enum MiltonInputFlags
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

struct MiltonInput
{
    int flags;  // MiltonInputFlags
    MiltonMode mode_to_set;

    v2i  points[MAX_INPUT_BUFFER_ELEMS];
    f32  pressures[MAX_INPUT_BUFFER_ELEMS];
    i32  input_count;

    v2i  click;
    v2i  hover_point;
    i32  scale;
    v2i  pan_delta;
};


enum SaveEnum
{
    SaveEnum_IN_USE,
    SaveEnum_GOOD_TO_GO,
};


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

void milton_init(MiltonState* milton_state, i32 width, i32 height);

// Expects absolute path
void milton_set_canvas_file(MiltonState* milton_state, PATH_CHAR* fname);
void milton_set_default_canvas_file(MiltonState* milton_state);

void milton_reset_canvas_and_set_default(MiltonState* milton_state);

void milton_gl_backend_draw(MiltonState* milton_state);

// Between 0 and k_max_brush_size
i32 milton_get_brush_radius(MiltonState* milton_state);
void milton_set_brush_size(MiltonState* milton_state, i32 size);
void milton_increase_brush_size(MiltonState* milton_state);
void milton_decrease_brush_size(MiltonState* milton_state);
float milton_get_pen_alpha(MiltonState* milton_state);
void milton_set_pen_alpha(MiltonState* milton_state, float alpha);

void milton_use_previous_mode(MiltonState* milton_state);
void milton_switch_mode(MiltonState* milton_state, MiltonMode mode);

// Our "game loop" inner function.
void milton_update_and_render(MiltonState* milton_state, MiltonInput* input);

// If memory has been requested after rendering failed, this function will realloc.
void milton_expand_render_memory(MiltonState* milton_state);

void milton_try_quit(MiltonState* milton_state);

void milton_new_layer(MiltonState* milton_state);
void milton_set_working_layer(MiltonState* milton_state, Layer* layer);
void milton_delete_working_layer(MiltonState* milton_state);
void milton_set_background_color(MiltonState* milton_state, v3f background_color);


static b32 milton_brush_smoothing_enabled(MiltonState* milton_state);
static void milton_toggle_brush_smoothing(MiltonState* milton_state);

