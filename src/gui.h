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

#include "utils.h"

enum class ColorPickerFlags {

    NOTHING = 0,

    WHEEL_ACTIVE    = (1 << 1),
    TRIANGLE_ACTIVE = (1 << 2)
};
DECLARE_FLAG(ColorPickerFlags);

/* void set_flag(ColorPickerFlags& f, ColorPickerFlags val) */
/* { */
/*     f = (ColorPickerFlags)((int)f | (int)val); */
/* } */

/* b32 check_flag(ColorPickerFlags flags, ColorPickerFlags f) */
/* { */
/*     return (b32)((int)flags & (int)f); */
/* } */

struct PickerData {
    v2f a;  // Corresponds to value = 0      (black)
    v2f b;  // Corresponds to saturation = 0 (white)
    v2f c;  // Points to chosen hue.         (full color)

    v3f  hsv;
};

struct ColorButton {
    i32 center_x;
    i32 center_y;
    i32 width;
    i32 height;
    v4f color;

    PickerData picker_data;

    ColorButton* next;
};

struct ColorPicker {
    v2i     center;  // In screen pixel coordinates.
    i32     bounds_radius_px;
    Rect    bounds;
    float   wheel_radius;
    float   wheel_half_width;

    u32*    pixels;  // Blit this to render picker

    PickerData info;

    ColorButton color_buttons;

    ColorPickerFlags flags;
};

enum class ColorPickResult {
    NOTHING         = 0,
    CHANGE_COLOR,
};


struct GuiButton {
    Rect            rect;
    Bitmap          bitmap;
};

// typedef'd in milton.h
struct MiltonGui {
    b32 active;  // `active == true` when gui currently owns all user input.
    b32 did_change_color;
    b32 did_hit_button;  // Avoid multiple clicks.

    ColorPicker picker;

    GuiButton brush_button;

    v2i preview_pos;  // If rendering brush preview, this is where to do it.
};

//
// GUI API
//
// Call from the main loop before milton_update
void milton_gui_tick(MiltonInputFlags& input, const MiltonState& milton_state);


//
void                gui_init(Arena* root_arena, MiltonGui* gui);
v3f                 gui_get_picker_rgb(MiltonGui* gui);
// Returns true if the GUI consumed input. False if the GUI wasn't affected
b32                 gui_consume_input(MiltonGui* gui, MiltonInput* input);
// Use if gui_consume_input was true and nothing else wants to capture input.
MiltonRenderFlags   gui_process_input(MiltonState* milton_state, MiltonInput* input);

// When a selected color is used in a stroke, call this to update the color
// button list.
b32  gui_mark_color_used(MiltonGui* gui, v3f stroke_color);
void gui_deactivate(MiltonGui* gui);

// Color Picker API
b32     picker_hits_wheel(ColorPicker* picker, v2f point);
float   picker_wheel_get_angle(ColorPicker* picker, v2f point);
v3f     picker_hsv_from_point(ColorPicker* picker, v2f point);
Rect    get_bounds_for_picker_and_colors(const ColorPicker& picker);
