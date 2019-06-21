// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "memory.h"
#include "system_includes.h"
#include "canvas.h"
#include "DArray.h"
#include "profiler.h"

#define STROKE_MAX_POINTS           2048
#define MILTON_DEFAULT_SCALE        (1 << 10)
#define NO_PRESSURE_INFO            -1.0f
#define MAX_INPUT_BUFFER_ELEMS      32
#define MILTON_MAX_BRUSH_SIZE       100
#define MILTON_HIDE_BRUSH_OVERLAY_AT_THIS_SIZE 12
#define HOVER_FLASH_THRESHOLD_MS    500  // How long does the hidden brush hover show when it has changed size.


struct MiltonGLState
{
    GLuint quad_program;
    GLuint texture;
    GLuint vbo;
};

enum class MiltonMode
{
    NONE,

    ERASER,
    PEN,
    PRIMITIVE, // Lines, circles, etc.
    EXPORTING,
    EYEDROPPER,
    HISTORY,
    PEEK_OUT,
    SELECT,
};

enum BrushEnum
{
    BrushEnum_PEN,
    BrushEnum_ERASER,
    BrushEnum_PRIMITIVE,

    BrushEnum_NOBRUSH,  // Non-painting modes

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
struct RenderBackend;
struct CanvasView;
struct Layer;
struct MiltonPersist;
struct MiltonBindings;
struct CopyPaste;

// Stuff than can be reset when unloading a canvas
struct CanvasState
{
    Arena  arena;

    i32         layer_guid;  // to create unique ids;
    Layer*      root_layer;
    Layer*      working_layer;

    DArray<HistoryElement> history;
    DArray<HistoryElement> redo_stack;
    //Layer**         layer_graveyard;
    DArray<Stroke>         stroke_graveyard;

    i32         stroke_id_count;
};

enum PrimitiveFSM
{
    Primitive_WAITING,
    Primitive_DRAWING,
    Primitive_DONE,
};

#pragma pack(push, 1)
struct MiltonSettings
{
    v3f background_color;
    float peek_out_increment;

    MiltonBindings bindings;
};
#pragma pack(pop)

struct Eyedropper
{
    u8* buffer;
};

struct SmoothFilter
{
    #define SMOOTHING_WINDOW 6
    v2i points[SMOOTHING_WINDOW];
    size_t index;
    size_t n_points;
};

struct PeekOut
{
    WallTime begin_anim_time;
    b32 peek_out_ended;

    i64 high_scale;
    i64 low_scale;

    v2l begin_pan;
    v2l end_pan;
};

struct RenderSettings
{
    b32 do_full_redraw;
};

struct Milton
{
    b32 flags;  // See MiltonStateFlags

#if MILTON_SAVE_ASYNC
    SDL_mutex*  save_mutex;
    i64         save_flag;   // See SaveEnum
    SDL_cond*   save_cond;
    SDL_Thread* save_thread;
#endif

    // ---- The Painting
    CanvasState*    canvas;
    CanvasView*     view;

    Eyedropper* eyedropper;

    Brush       brushes[BrushEnum_COUNT];
    i32         brush_sizes[BrushEnum_COUNT];  // In screen pixels

    Stroke      working_stroke;
    // ----  // gui->picker.info also stored


    v2i hover_point;  // Track the pointer when not stroking..
    i32 hover_flash_ms;  // Set on keyboard shortcut to change brush size.
                        // Brush hover "flashes" if it is currently hidden to show its current size.

    PeekOut* peek_out;

    // Read only
    // Set these with milton_switch_mode and milton_leave_mode
    MiltonMode current_mode;
    MiltonMode last_mode;

    PrimitiveFSM primitive_fsm;

    SmoothFilter* smooth_filter;

    RenderSettings render_settings;
    RenderBackend* renderer;

    // Heap
    Arena       root_arena;     // Lives forever
    Arena       canvas_arena;   // Gets reset every canvas.

    // Subsystems
    MiltonGLState* gl;
    MiltonGui* gui;
    MiltonSettings* settings;  // User settings
    MiltonPersist* persist;
    CopyPaste* pasta;

#if MILTON_ENABLE_PROFILING
    b32 viz_window_visible;
    GraphData graph_frame;
#endif
};

enum MiltonStateFlags
{
    MiltonStateFlags_RUNNING                = 1 << 0,
                                           // 1 << 1 unused
    MiltonStateFlags_FULL_REDRAW_REQUESTED = 1 << 2,
    MiltonStateFlags_JUST_SAVED             = 1 << 3,
    MiltonStateFlags_NEW_CANVAS             = 1 << 4,
    MiltonStateFlags_DEFAULT_CANVAS         = 1 << 5,
    MiltonStateFlags_IGNORE_NEXT_CLICKUP    = 1 << 6,  // When selecting eyedropper from menu, avoid the click from selecting the color...
                                           // 1 << 7 unused
                                           // 1 << 8 unused
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
                                        // 1 << 4 free to use
                                        // 1 << 5 free to use
    MiltonInputFlags_HOVERING            = 1 << 6,
    MiltonInputFlags_PANNING             = 1 << 7,
    MiltonInputFlags_IMGUI_GRABBED_INPUT = 1 << 8,
    MiltonInputFlags_SAVE_FILE           = 1 << 9,
    MiltonInputFlags_OPEN_FILE           = 1 << 10,
    MiltonInputFlags_CLICK               = 1 << 11,
    MiltonInputFlags_CLICKUP             = 1 << 12,
};

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
};


enum SaveEnum
{
    SaveEnum_WAITING,
    SaveEnum_SAVE_REQUESTED,
    SaveEnum_KILL,
};

enum MiltonInitFlags
{
    MiltonInit_DEFAULT = 0,
    MiltonInit_FOR_TEST = 1<<0,  // No graphics layer. No reading from disk
};
void milton_init(Milton* milton, i32 width, i32 height, f32 ui_scale, PATH_CHAR* file_to_open, MiltonInitFlags init_flags = MiltonInit_DEFAULT);

// Expects absolute path
void milton_set_canvas_file(Milton* milton, PATH_CHAR* fname);
void milton_set_default_canvas_file(Milton* milton);
void milton_set_last_canvas_fname(PATH_CHAR* last_fname);
void milton_unset_last_canvas_fname();

void milton_save_postlude(Milton* milton);


void milton_reset_canvas(Milton* milton);
void milton_reset_canvas_and_set_default(Milton* milton);

void milton_gl_backend_draw(Milton* milton);

b32 current_mode_is_for_drawing(Milton* milton);

i32     milton_get_brush_radius(Milton* milton);   // Between 0 and k_max_brush_size
void    milton_set_brush_size(Milton* milton, i32 size);
void    milton_increase_brush_size(Milton* milton);
void    milton_decrease_brush_size(Milton* milton);
float   milton_get_brush_alpha(Milton* milton);
void    milton_set_brush_alpha(Milton* milton, float alpha);

// Returns false if the pan_delta moves the pan vector outside of the canvas.
void milton_resize_and_pan(Milton* milton, v2i pan_delta, v2i new_screen_size);

void milton_leave_mode(Milton* milton);
void milton_switch_mode(Milton* milton, MiltonMode mode);

void milton_escape(Milton* milton);

// Our "game loop" inner function.
void milton_update_and_render(Milton* milton, MiltonInput* input);

void milton_try_quit(Milton* milton);

void milton_new_layer(Milton* milton);
void milton_new_layer_with_id(Milton* milton, i32 new_id);
void milton_set_working_layer(Milton* milton, Layer* layer);
void milton_delete_working_layer(Milton* milton);
void milton_set_background_color(Milton* milton, v3f background_color);

// Set the center of the zoom
void milton_set_zoom_at_point(Milton* milton, v2i zoom_center);
void milton_set_zoom_at_screen_center(Milton* milton);

b32  milton_brush_smoothing_enabled(Milton* milton);
void milton_toggle_brush_smoothing(Milton* milton);


void peek_out_trigger_start(Milton* milton);
void peek_out_trigger_stop(Milton* milton);
