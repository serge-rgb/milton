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


typedef enum {
    ColorPickerFlags_NOTHING = 0,

    ColorPickerFlags_WHEEL_ACTIVE    = (1 << 1),
    ColorPickerFlags_TRIANGLE_ACTIVE = (1 << 2)
} ColorPickerFlags;

// TODO: There should be a way to deal with high density displays.
typedef struct ColorButton_s ColorButton;
struct ColorButton_s {
    i32 center_x;
    i32 center_y;
    i32 width;
    i32 height;
    v4f color;

    ColorButton* next;
};

typedef struct ColorPicker_s {
    v2f a;  // Corresponds to value = 0      (black)
    v2f b;  // Corresponds to saturation = 0 (white)
    v2f c;  // Points to chosen hue.         (full color)

    v2i     center;  // In screen pixel coordinates.
    i32     bounds_radius_px;
    Rect    bounds;
    float   wheel_radius;
    float   wheel_half_width;

    u32*    pixels;  // Blit this to render picker

    v3f     hsv;

    ColorButton color_buttons;

    ColorPickerFlags flags;

} ColorPicker;

typedef enum {
    ColorPickResult_NOTHING         = 0,
    ColorPickResult_CHANGE_COLOR    = (1 << 1),
} ColorPickResult;


func b32 picker_hits_wheel(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = sub_v2f (point, center);
    float dist = magnitude(arrow);

    b32 hits = (dist <= picker->wheel_radius + picker->wheel_half_width ) &&
               (dist >= picker->wheel_radius - picker->wheel_half_width );

    return hits;
}

func float picker_wheel_get_angle(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = sub_v2f (point, center);
    v2f baseline = { 1, 0 };
    float dotp = (DOT(arrow, baseline)) / (magnitude(arrow));
    float angle = acosf(dotp);
    if (point.y > center.y) {
        angle = (2 * kPi) - angle;
    }
    return angle;
}

func void picker_update_wheel(ColorPicker* picker, v2f point)
{
    float angle = picker_wheel_get_angle(picker, point);
    picker->hsv.h = radians_to_degrees(angle);
    // Update the triangle
    {
        float radius = 0.9f * (picker->wheel_radius - picker->wheel_half_width);
        v2f center = v2i_to_v2f(picker->center);
        {
            v2f point = polar_to_cartesian(-angle, radius);
            point = add_v2f(point, center);
            picker->c = point;
        }
        {
            v2f point = polar_to_cartesian(-angle + 2 * kPi / 3.0f, radius);
            point = add_v2f(point, center);
            picker->b = point;
        }
        {
            v2f point = polar_to_cartesian(-angle + 4 * kPi / 3.0f, radius);
            point = add_v2f(point, center);
            picker->a = point;
        }
    }
}


func b32 picker_hits_triangle(ColorPicker* picker, v2f fpoint)
{
    b32 result = is_inside_triangle(fpoint, picker->a, picker->b, picker->c);
    return result;
}

func void picker_deactivate(ColorPicker* picker)
{
    picker->flags = ColorPickerFlags_NOTHING;
}

func b32 is_inside_picker_rect(ColorPicker* picker, v2i point)
{
    return is_inside_rect(picker->bounds, point);
}

func b32 is_inside_picker_active_area(ColorPicker* picker, v2i point)
{
    v2f fpoint = v2i_to_v2f(point);
    b32 result = picker_hits_wheel(picker, fpoint) ||
                 is_inside_triangle(fpoint, picker->a, picker->b, picker->c);
    return result;
}

func b32 is_picker_accepting_input(ColorPicker* picker, v2i point)
{
    // If wheel is active, yes! Gimme input.
    if (picker->flags & ColorPickerFlags_WHEEL_ACTIVE) {
        return true;
    } else {
        //return is_inside_picker_active_area(picker, point);
        return is_inside_picker_rect(picker, point);
    }
}

func Rect picker_get_bounds(ColorPicker* picker)
{
    Rect picker_rect;
    {
        picker_rect.left   = picker->center.x - picker->bounds_radius_px;
        picker_rect.right  = picker->center.x + picker->bounds_radius_px;
        picker_rect.bottom = picker->center.y + picker->bounds_radius_px;
        picker_rect.top    = picker->center.y - picker->bounds_radius_px;
    }
    assert (picker_rect.left >= 0);
    assert (picker_rect.top >= 0);

    return picker_rect;
}

func v3f picker_hsv_from_point(ColorPicker* picker, v2f point)
{
    float area = orientation(picker->a, picker->b, picker->c);
    assert (area != 0);
    float inv_area = 1.0f / area;
    float s = orientation(picker->b, point, picker->a) * inv_area;
    if (s > 1) { s = 1; }
    if (s < 0) { s = 0; }
    float v = 1 - (orientation(point, picker->c, picker->a) * inv_area);
    if (v > 1) { v = 1; }
    if (v < 0) { v = 0; }

    v3f hsv = {
        picker->hsv.h,
        s,
        v,
    };
    return hsv;
}

func ColorPickResult picker_update(ColorPicker* picker, v2i point)
{
    ColorPickResult result = ColorPickResult_NOTHING;
    v2f fpoint = v2i_to_v2f(point);
    if (picker->flags == ColorPickerFlags_NOTHING) {
        if (picker_hits_wheel(picker, fpoint)) {
            picker->flags |= ColorPickerFlags_WHEEL_ACTIVE;
        }
    }
    if (picker->flags & ColorPickerFlags_WHEEL_ACTIVE) {
        if (!(picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE)) {
            picker_update_wheel(picker, fpoint);
            result |= ColorPickResult_CHANGE_COLOR;
        }
    }
    if (picker_hits_triangle(picker, fpoint)) {
        if (!(picker->flags & ColorPickerFlags_WHEEL_ACTIVE)) {
            picker->flags |= ColorPickerFlags_TRIANGLE_ACTIVE;
            picker->hsv = picker_hsv_from_point(picker, fpoint);
            result |= ColorPickResult_CHANGE_COLOR;
        }
    } else if (picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE) {
        // We don't want the chooser to "stick" if go outside the triangle
        // (i.e. picking black should be easy)
        v2f segments[] = {
            picker->a, picker->b,
            picker->b, picker->c,
            picker->c, picker->a,
        };

        for (i32 segment_i = 0; segment_i < 6; segment_i += 2) {
            v2i a = v2f_to_v2i(segments[segment_i    ]);
            v2i b = v2f_to_v2i(segments[segment_i + 1]);
            v2f intersection = { 0 };
            b32 hit = intersect_line_segments(point, picker->center,
                                              a, b,
                                              &intersection);
            if (hit) {
                picker->hsv = picker_hsv_from_point(picker, intersection);
                result |= ColorPickResult_CHANGE_COLOR;
                break;
            }
        }
    }

    return result;
}

func void picker_init(ColorPicker* picker)
{

    v2f fpoint = {
        (f32)picker->center.x + (int)(picker->wheel_radius),
        (f32)picker->center.y
    };
    picker_update_wheel(picker, fpoint);
    picker->hsv = (v3f){ 0, 1, 1 };
}

////////////////////////////////////
// API
////////////////////////////////////


// typedef'd in milton.h
struct MiltonGui_s {
    b32 active;  // `active == true` when gui currently owns all user input.
    b32 did_change_color;

    ColorPicker picker;
};

func v3f gui_get_picker_rgb(MiltonGui* gui)
{
    v3f rgb = hsv_to_rgb(gui->picker.hsv);
    return rgb;
}

func b32 gui_accepts_input(MiltonGui* gui, MiltonInput* input)
{
    v2i point = input->points[0];
    b32 accepts = is_picker_accepting_input(&gui->picker, point);
    return accepts;
}

func MiltonRenderFlags gui_update(MiltonState* milton_state, MiltonInput* input)
{
    MiltonRenderFlags render_flags = MiltonRenderFlags_NONE;
    v2i point = input->points[0];
    ColorPickResult pick_result = picker_update(&milton_state->gui->picker, point);
    if (pick_result & ColorPickResult_CHANGE_COLOR &&
            milton_state->current_mode == MiltonMode_PEN) {
        milton_state->gui->did_change_color = true;
        v3f rgb = hsv_to_rgb(milton_state->gui->picker.hsv);
        milton_state->brushes[BrushEnum_PEN].color =
                to_premultiplied(rgb, milton_state->brushes[BrushEnum_PEN].alpha);
    }
    render_flags |= MiltonRenderFlags_PICKER_UPDATED;
    milton_state->gui->active = true;
    return render_flags;
}

func void gui_init(Arena* root_arena, MiltonGui* gui)
{
    i32 bounds_radius_px = 100;
    f32 wheel_half_width = 12;
    gui->picker.center = (v2i){ bounds_radius_px + 20, bounds_radius_px + 20 };
    gui->picker.bounds_radius_px = bounds_radius_px;
    gui->picker.wheel_half_width = wheel_half_width;
    gui->picker.wheel_radius = (f32)bounds_radius_px - 5.0f - wheel_half_width;
    gui->picker.hsv = (v3f){ 0.0f, 1.0f, 0.7f };
    Rect bounds;
    bounds.left = gui->picker.center.x - bounds_radius_px;
    bounds.right = gui->picker.center.x + bounds_radius_px;
    bounds.top = gui->picker.center.y - bounds_radius_px;
    bounds.bottom = gui->picker.center.y + bounds_radius_px;
    gui->picker.bounds = bounds;
    gui->picker.pixels = arena_alloc_array(root_arena,
                                           (4 * bounds_radius_px * bounds_radius_px),
                                           u32);
    picker_init(&gui->picker);

    i32 spacing = 4;
    i32 num_buttons = 5;

    i32 button_size = (2*bounds_radius_px - (num_buttons - 1) * spacing) / num_buttons;

    i32 current_center_x = 40;

    ColorButton* cur_button = &gui->picker.color_buttons;
    for (i32 i = 0; i < num_buttons; ++i) {
        assert (cur_button->next == NULL);

        cur_button->center_x = current_center_x;
        cur_button->center_y = gui->picker.center.y + bounds_radius_px + 40;
        cur_button->width = button_size / 2;
        cur_button->height = button_size / 2;
        cur_button->color = (v4f){0.0, 0.0, 0.0, 0.0};

        current_center_x += spacing + button_size;

        if (i != (num_buttons - 1)) {
            cur_button->next = arena_alloc_elem(root_arena, ColorButton);
        }
        cur_button = cur_button->next;
    }
}

func void gui_deactivate(MiltonGui* gui)
{
    if (gui->did_change_color) {
        ColorButton* button = &gui->picker.color_buttons;
        v4f color = button->color;
        v3f rgb  = hsv_to_rgb(gui->picker.hsv);
        button->color.rgb = rgb;
        button->color.a = 1.0f;
        button = button->next;

        while (button) {
            v4f tmp_color = button->color;
            button->color = color;
            color = tmp_color;
            button = button->next;
        }
    }

    picker_deactivate(&gui->picker);

    // Reset transient values
    gui->active = false;
    gui->did_change_color = false;
}
