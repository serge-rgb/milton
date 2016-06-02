// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

enum ColorPickerFlags
{
    ColorPickerFlags_NOTHING = 0,

    ColorPickerFlags_WHEEL_ACTIVE    = (1 << 1),
    ColorPickerFlags_TRIANGLE_ACTIVE = (1 << 2)
} ColorPickerFlags;

typedef struct PickerData
{
    v2f a;  // Corresponds to value = 0      (black)
    v2f b;  // Corresponds to saturation = 0 (white)
    v2f c;  // Points to chosen hue.         (full color)

    v3f  hsv;
} PickerData;

typedef struct ColorButton_s ColorButton;
struct ColorButton_s
{
    i32 x;
    i32 y;
    i32 w;
    i32 h;

    v4f rgba;

    ColorButton* next;
};

typedef struct ColorPicker
{
    v2i     center;  // In screen pixel coordinates.
    i32     bounds_radius_px;
    Rect    bounds;
    float   wheel_radius;
    float   wheel_half_width;

    u32*    pixels;  // Blit this to render picker

    PickerData data;

    ColorButton color_buttons;

    int flags;  // ColorPickerFlags
} ColorPicker;

typedef enum
{
    ColorPickResult_NOTHING,
    ColorPickResult_CHANGE_COLOR,
} ColorPickResult;


typedef struct
{
    Rect            rect;
    Bitmap          bitmap;
} GuiButton;

typedef enum
{
    ExporterState_EMPTY,
    ExporterState_GROWING_RECT,
    ExporterState_SELECTED,
} ExporterState;

struct Exporter
{
    ExporterState state;
    // Pivot: The raster point where we click to begin the rectangle
    v2i pivot;
    // Needle, the point that we drag.
    v2i needle;

    int scale;
};
typedef struct Exporter Exporter;

// State machine for gui
enum MiltonGuiFlags
{
    MiltonGuiFlags_NONE,

    MiltonGuiFlags_SHOWING_PREVIEW   = 1 << 0,
    MiltonGuiFlags_NEEDS_REDRAW      = 1 << 1,
} MiltonGuiFlags;

typedef struct MiltonGui
{
    b32 visible;
    b32 show_help_widget;

    b32 active;  // `active == true` when gui currently owns all user input.
    b32 did_hit_button;  // Avoid multiple clicks.

    int flags;  // MiltonGuiFlags

    ColorPicker picker;

    Exporter exporter;

    v2i preview_pos;  // If rendering brush preview, this is where to do it.
    v2i preview_pos_prev;  // Keep the previous position to clear the canvas.
} MiltonGui;

//
// GUI API
//
// Call from the main loop before milton_update
typedef struct PlatformState PlatformState;
void milton_imgui_tick(MiltonInput* input, PlatformState* platform_state,  MiltonState* milton_state);


//
void                gui_init(Arena* root_arena, MiltonGui* gui);
void                gui_toggle_visibility(MiltonGui* gui);
void                gui_toggle_help(MiltonGui* gui);
v3f                 gui_get_picker_rgb(MiltonGui* gui);
// Returns true if the GUI consumed input. False if the GUI wasn't affected
b32                 picker_consume_input(MiltonGui* gui, MiltonInput* input);
// Use if picker_consume_input was true and nothing else wants to capture input.
/*MiltonRenderFlags*/   int gui_process_input(MiltonState* milton_state, MiltonInput* input);
void                gui_imgui_set_ungrabbed(MiltonGui* gui);

void exporter_init(Exporter* exporter);
b32 exporter_input(Exporter* exporter, MiltonInput* input);  // True if exporter changed


// Color Picker API
void    picker_init(ColorPicker* picker);
b32     picker_hits_wheel(ColorPicker* picker, v2f point);
float   picker_wheel_get_angle(ColorPicker* picker, v2f point);
v3f     picker_hsv_from_point(ColorPicker* picker, v2f point);
Rect    picker_get_bounds(ColorPicker* picker);
Rect    get_bounds_for_picker_and_colors(ColorPicker* picker);
// When a selected color is used in a stroke, call this to update the color
// button list.
b32  gui_mark_color_used(MiltonGui* gui);
void gui_deactivate(MiltonGui* gui);

// Eye Dropper
void eyedropper_input(MiltonGui* gui, u32* canvas_buffer, i32 w, i32 h, v2i point);

