// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "utils.h"
#include "vector.h"

struct Arena;
struct MiltonInput;
struct Milton;
struct PlatformState;
struct MiltonSettings;

enum ColorPickerFlags
{
    ColorPickerFlags_NOTHING = 0,

    ColorPickerFlags_WHEEL_ACTIVE    = (1 << 1),
    ColorPickerFlags_TRIANGLE_ACTIVE = (1 << 2)
};

struct PickerData
{
    v2f a;  // Corresponds to value = 0      (black)
    v2f b;  // Corresponds to saturation = 0 (white)
    v2f c;  // Points to chosen hue.         (full color)

    v3f  hsv;
};

struct ColorButton
{
    i32 x;
    i32 y;
    i32 w;
    i32 h;

    v4f rgba;

    ColorButton* next;
};

struct ColorPicker
{
    v2i     center;  // In screen pixel coordinates.
    i32     bounds_radius_px;
    Rect    bounds;
    float   wheel_radius;
    float   wheel_half_width;

    u32*    pixels;  // Blit this to render picker. Dimensions: picker_get_bounds(..)

    PickerData data;

    ColorButton* color_buttons;

    int flags;  // ColorPickerFlags
};

enum ColorPickResult
{
    ColorPickResult_NOTHING,
    ColorPickResult_CHANGE_COLOR,
};

struct GuiButton
{
    Rect            rect;
    Bitmap          bitmap;
};

enum ExporterState
{
    ExporterState_EMPTY,
    ExporterState_GROWING_RECT,
    ExporterState_SELECTED,
};

struct Exporter
{
    ExporterState state;
    // Pivot: The raster point where we click to begin the rectangle
    v2i pivot;
    // Needle, the point that we drag.
    v2i needle;

    int scale;
};

// State machine for gui
enum MiltonGuiFlags
{
    MiltonGuiFlags_NONE,

    MiltonGuiFlags_SHOWING_PREVIEW   = 1 << 0,
};

struct MiltonGui
{
    b32 menu_visible;
    b32 visible;
    b32 show_help_widget;

    b32 owns_user_input;
    b32 did_hit_button;  // Avoid multiple clicks.

    int flags;  // MiltonGuiFlags

    i32 history;

    ColorPicker picker;

    Exporter exporter;

    v2i preview_pos;  // If rendering brush preview, this is where to do it.
    v2i preview_pos_prev;  // Keep the previous position to clear the canvas.

    f32 scale;

    MiltonSettings* original_settings;

    char scratch_binding_key[Action_COUNT][2];
};

//
// GUI API
//
// Call from the main loop before milton_update

void milton_imgui_tick(MiltonInput* input, PlatformState* platform_state,  Milton* milton);



//
void                gui_init(Arena* root_arena, MiltonGui* gui, f32 scale);
void                gui_toggle_menu_visibility(MiltonGui* gui);
void                gui_toggle_help(MiltonGui* gui);
v3f                 gui_get_picker_rgb(MiltonGui* gui);

// Returns true if the GUI consumed input. False if the GUI wasn't affected
b32                 gui_consume_input(MiltonGui* gui, MiltonInput const* input);
void                gui_imgui_set_ungrabbed(MiltonGui* gui);
void                gui_picker_from_rgb(ColorPicker* picker, v3f rgb);

void exporter_init(Exporter* exporter);
b32 exporter_input(Exporter* exporter, MiltonInput* input);  // True if exporter changed

// Returns true if point is over a GUI element
b32 gui_point_hovers(MiltonGui* gui, v2i point);


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

