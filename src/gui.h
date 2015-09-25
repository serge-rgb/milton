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

typedef struct PickerData_s {
    v2f a;  // Corresponds to value = 0      (black)
    v2f b;  // Corresponds to saturation = 0 (white)
    v2f c;  // Points to chosen hue.         (full color)

    v3f  hsv;
} PickerData;

// TODO: There should be a way to deal with high density displays.
typedef struct ColorButton_s ColorButton;
struct ColorButton_s {
    i32 center_x;
    i32 center_y;
    i32 width;
    i32 height;
    v4f color;

    PickerData picker_data;

    ColorButton* next;
};

typedef struct ColorPicker_s {
    v2i     center;  // In screen pixel coordinates.
    i32     bounds_radius_px;
    Rect    bounds;
    float   wheel_radius;
    float   wheel_half_width;

    u32*    pixels;  // Blit this to render picker

    PickerData info;

    ColorButton color_buttons;

    ColorPickerFlags flags;

} ColorPicker;

typedef enum {
    ColorPickResult_NOTHING         = 0,
    ColorPickResult_CHANGE_COLOR    = (1 << 1),
} ColorPickResult;

Rect color_button_as_rect(ColorButton* button)
{
    Rect rect = (Rect) {
        .left = button->center_x - button->width,
        .right = button->center_x + button->width,
        .top = button->center_y - button->height,
        .bottom = button->center_y + button->height,
    };
    return rect;
}

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
    picker->info.hsv.h = radians_to_degrees(angle);
    // Update the triangle
    {
        float radius = 0.9f * (picker->wheel_radius - picker->wheel_half_width);
        v2f center = v2i_to_v2f(picker->center);
        {
            v2f point = polar_to_cartesian(-angle, radius);
            point = add_v2f(point, center);
            picker->info.c = point;
        }
        {
            v2f point = polar_to_cartesian(-angle + 2 * kPi / 3.0f, radius);
            point = add_v2f(point, center);
            picker->info.b = point;
        }
        {
            v2f point = polar_to_cartesian(-angle + 4 * kPi / 3.0f, radius);
            point = add_v2f(point, center);
            picker->info.a = point;
        }
    }
}


func b32 picker_hits_triangle(ColorPicker* picker, v2f fpoint)
{
    b32 result = is_inside_triangle(fpoint, picker->info.a, picker->info.b, picker->info.c);
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
                 is_inside_triangle(fpoint, picker->info.a, picker->info.b, picker->info.c);
    return result;
}

func b32 is_inside_picker_button_area(ColorPicker* picker, v2i point)
{
    static b32 rect_is_cached;
    static Rect button_rect;
    if (!rect_is_cached) {
        button_rect = (Rect) { 0 };
        ColorButton* button = &picker->color_buttons;
        while (button) {
            button_rect = rect_union(button_rect,
                                     color_button_as_rect(button));
            button = button->next;
        }
    }
    b32 is_inside = is_inside_rect(button_rect, point);
    return is_inside;
}

func b32 is_picker_accepting_input(ColorPicker* picker, v2i point)
{
    // If wheel is active, yes! Gimme input.
    if (picker->flags & ColorPickerFlags_WHEEL_ACTIVE) {
        return true;
    } else if (is_inside_picker_rect(picker, point)) {
        //return is_inside_picker_active_area(picker, point);
        return true;
    } else {
        return is_inside_picker_button_area(picker, point);
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
    float area = orientation(picker->info.a, picker->info.b, picker->info.c);
    assert (area != 0);
    float inv_area = 1.0f / area;
    float s = orientation(picker->info.b, point, picker->info.a) * inv_area;
    if (s > 1) { s = 1; }
    if (s < 0) { s = 0; }
    float v = 1 - (orientation(point, picker->info.c, picker->info.a) * inv_area);
    if (v > 1) { v = 1; }
    if (v < 0) { v = 0; }

    v3f hsv = {
        picker->info.hsv.h,
        s,
        v,
    };
    return hsv;
}

func b32 picker_hit_history_buttons(ColorPicker* picker, v2i point)
{
    b32 hits = false;
    ColorButton* first = &picker->color_buttons;
    ColorButton* prev = NULL;
    ColorButton* button = first;
    while (button) {
        if ( button->color.a != 0 &&
             is_inside_rect(color_button_as_rect(button), point) ) {
            hits = true;
            picker->info = button->picker_data;
            // Swap info with the first.
            v4f swp_color = button->color;
            PickerData swp_data = button->picker_data;
            button->color = first->color;
            button->picker_data = first->picker_data;
            first->color = swp_color;
            first->picker_data = swp_data;

            break;
        }
        prev = button;
        button = button->next;
    }
    return hits;
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
            picker->info.hsv = picker_hsv_from_point(picker, fpoint);
            result |= ColorPickResult_CHANGE_COLOR;
        }
    } else if (picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE) {
        // We don't want the chooser to "stick" if go outside the triangle
        // (i.e. picking black should be easy)
        v2f segments[] = {
            picker->info.a, picker->info.b,
            picker->info.b, picker->info.c,
            picker->info.c, picker->info.a,
        };

        for (i32 segment_i = 0; segment_i < 6; segment_i += 2) {
            v2i a = v2f_to_v2i(segments[segment_i    ]);
            v2i b = v2f_to_v2i(segments[segment_i + 1]);
            v2f intersection = { 0 };
            b32 hit = intersect_line_segments(point, picker->center,
                                              a, b,
                                              &intersection);
            if (hit) {
                picker->info.hsv = picker_hsv_from_point(picker, intersection);
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
    picker->info.hsv = (v3f){ 0, 1, 1 };
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
    v3f rgb = hsv_to_rgb(gui->picker.info.hsv);
    return rgb;
}

// Returns true if the GUI consumed input. False if the GUI wasn't affected
func b32 gui_consume_input(MiltonGui* gui, MiltonInput* input)
{
    v2i point = input->points[0];
    b32 accepts = is_picker_accepting_input(&gui->picker, point);
    accepts |= picker_hit_history_buttons(&gui->picker, point);
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
        v3f rgb = hsv_to_rgb(milton_state->gui->picker.info.hsv);
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
    gui->picker.info.hsv = (v3f){ 0.0f, 1.0f, 0.7f };
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

// When a selected color is used in a stroke, call this to update the color
// button list.
func b32 gui_mark_color_used(MiltonGui* gui, v3f stroke_color)
{
    b32 added = false;
    ColorButton* button = &gui->picker.color_buttons;

    v3f picker_color  = hsv_to_rgb(gui->picker.info.hsv);
    if (!equals_v3f(picker_color, button->color.rgb)) {
        added = true;
        v4f button_color = color_rgb_to_rgba(picker_color,1);
        PickerData picker_data = gui->picker.info;
        // Pass info to the next one.
        while (button) {
            v4f tmp_color = button->color;
            PickerData tmp_data = button->picker_data;
            button->color = button_color;
            button->picker_data = picker_data;
            button_color = tmp_color;
            picker_data = tmp_data;
            button = button->next;
        }
    }

    return added;
}

func void gui_deactivate(MiltonGui* gui)
{
    picker_deactivate(&gui->picker);

    // Reset transient values
    gui->active = false;
    gui->did_change_color = false;
}
