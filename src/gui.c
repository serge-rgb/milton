// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "gui.h"

#include "milton.h"
#include "utils.h"


static Rect color_button_as_rect(const ColorButton* button)
{
    Rect rect = {0};
    rect.left = button->x;
    rect.right = button->x + button->w;
    rect.top = button->y;
    rect.bottom = button->y + button->h;
    return rect;
}

static void picker_update_points(ColorPicker* picker, float angle)
{
    picker->info.hsv.h = radians_to_degrees(angle);
    // Update the triangle
    float radius = 0.9f * (picker->wheel_radius - picker->wheel_half_width);
    v2f center = v2i_to_v2f(picker->center);
    {
        v2f point = polar_to_cartesian(-angle, radius);
        point = add2f(point, center);
        picker->info.c = point;
    }
    {
        v2f point = polar_to_cartesian(-angle + 2 * kPi / 3.0f, radius);
        point = add2f(point, center);
        picker->info.b = point;
    }
    {
        v2f point = polar_to_cartesian(-angle + 4 * kPi / 3.0f, radius);
        point = add2f(point, center);
        picker->info.a = point;
    }
}


static void picker_update_wheel(ColorPicker* picker, v2f polar_point)
{
    float angle = picker_wheel_get_angle(picker, polar_point);
    picker_update_points(picker, angle);
}

static b32 picker_hits_triangle(ColorPicker* picker, v2f fpoint)
{
    b32 result = is_inside_triangle(fpoint, picker->info.a, picker->info.b, picker->info.c);
    return result;
}

static void picker_deactivate(ColorPicker* picker)
{
    picker->flags = ColorPickerFlags_NOTHING;
}

static b32 is_inside_picker_rect(ColorPicker* picker, v2i point)
{
    return is_inside_rect(picker->bounds, point);
}

static Rect picker_color_buttons_bounds(const ColorPicker* picker)
{
    Rect bounds = {0};
    bounds.right = INT_MIN;
    bounds.left = INT_MAX;
    bounds.top = INT_MAX;
    bounds.bottom = INT_MIN;
    const ColorButton* button = &picker->color_buttons;
    while (button) {
        bounds = rect_union(bounds, color_button_as_rect(button));
        button = button->next;
    }
    return bounds;
}

static b32 is_inside_picker_button_area(ColorPicker* picker, v2i point)
{
    Rect button_rect = picker_color_buttons_bounds(picker);
    b32 is_inside = is_inside_rect(button_rect, point);
    return is_inside;
}

static b32 picker_is_accepting_input(ColorPicker* picker, v2i point)
{
    // If wheel is active, yes! Gimme input.
    if (check_flag(picker->flags, ColorPickerFlags_WHEEL_ACTIVE)) {
        return true;
    } else if (is_inside_picker_rect(picker, point)) {
        //return is_inside_picker_active_area(picker, point);
        return true;
    } else {
        return is_inside_picker_button_area(picker, point);
    }
}

static b32 picker_hit_history_buttons(ColorPicker* picker, v2i point)
{
    b32 hits = false;
    ColorButton* first = &picker->color_buttons;
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
        button = button->next;
    }
    return hits;
}

#if 0
static b32 is_inside_picker_active_area(ColorPicker* picker, v2i point)
{
    v2f fpoint = v2i_to_v2f(point);
    b32 result = picker_hits_wheel(picker, fpoint) ||
                 is_inside_triangle(fpoint, picker->info.a, picker->info.b, picker->info.c);
    return result;
}
#endif

static ColorPickResult picker_update(ColorPicker* picker, v2i point)
{
    ColorPickResult result = ColorPickResult_NOTHING;
    v2f fpoint = v2i_to_v2f(point);
    if (picker->flags == ColorPickerFlags_NOTHING) {
        if (picker_hits_wheel(picker, fpoint)) {
            set_flag(picker->flags, ColorPickerFlags_WHEEL_ACTIVE);
        }
    }
    if (check_flag(picker->flags, ColorPickerFlags_WHEEL_ACTIVE)) {
        if (!(check_flag(picker->flags, ColorPickerFlags_TRIANGLE_ACTIVE))) {
            picker_update_wheel(picker, fpoint);
            result = ColorPickResult_CHANGE_COLOR;
        }
    }
    if (picker_hits_triangle(picker, fpoint)) {
        if (!(check_flag(picker->flags, ColorPickerFlags_WHEEL_ACTIVE))) {
            set_flag(picker->flags, ColorPickerFlags_TRIANGLE_ACTIVE);
            picker->info.hsv = picker_hsv_from_point(picker, fpoint);
            result = ColorPickResult_CHANGE_COLOR;
        }
    } else if (check_flag(picker->flags, ColorPickerFlags_TRIANGLE_ACTIVE)) {
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
                result = ColorPickResult_CHANGE_COLOR;
                break;
            }
        }
    }

    return result;
}


b32 picker_hits_wheel(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = sub2f(point, center);
    float dist = magnitude(arrow);

    b32 hits = (dist <= picker->wheel_radius + picker->wheel_half_width ) &&
               (dist >= picker->wheel_radius - picker->wheel_half_width );

    return hits;
}

float picker_wheel_get_angle(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = sub2f(point, center);
    v2f baseline = { 1, 0 };
    float dotp = (DOT(arrow, baseline)) / (magnitude(arrow));
    float angle = acosf(dotp);
    if (point.y > center.y) {
        angle = (2 * kPi) - angle;
    }
    return angle;
}

static void picker_init(ColorPicker* picker)
{
    v2f fpoint = {
        (f32)picker->center.x + (int)(picker->wheel_radius),
        (f32)picker->center.y
    };
    picker_update_wheel(picker, fpoint);
    picker->info.hsv = (v3f){ 0, 1, 1 };
}

static void picker_from_rgb(ColorPicker* picker, v3f rgb)
{
    v3f hsv = rgb_to_hsv(rgb);
    picker->info.hsv = hsv;
    float angle = hsv.h * 2*kPi;
    picker_update_points(picker, angle);
}

static b32 picker_is_active(ColorPicker* picker)
{
    b32 is_active = check_flag(picker->flags, ColorPickerFlags_WHEEL_ACTIVE) || check_flag(picker->flags, ColorPickerFlags_TRIANGLE_ACTIVE);

    return is_active;
}

Rect picker_get_bounds(ColorPicker* picker)
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

void eyedropper_input(MiltonGui* gui, u32* canvas_buffer, i32 w, i32 h, v2i point)
{
    v4f color = color_u32_to_v4f(canvas_buffer[point.y*w+point.x]);

    picker_from_rgb(&gui->picker, color.rgb);
}

static void exporter_init(Exporter* exporter)
{
    *exporter = (Exporter){0};
    exporter->scale = 1;
}

void exporter_input(Exporter* exporter, MiltonInput* input)
{
    if ( input->input_count > 0 ) {
        v2i point = input->points[input->input_count - 1];
        if ( exporter->state == ExporterState_EMPTY ||
             exporter->state == ExporterState_SELECTED ) {
            exporter->pivot = point;
            exporter->needle = point;
            exporter->state = ExporterState_GROWING_RECT;
        } else if ( exporter->state == ExporterState_GROWING_RECT) {
            exporter->needle = point;
        }
    }
    if ( check_flag(input->flags, MiltonInputFlags_END_STROKE) && exporter->state != ExporterState_EMPTY ) {
        exporter->state = ExporterState_SELECTED;
    }
}

void gui_imgui_set_ungrabbed(MiltonGui* gui)
{
    unset_flag(gui->flags, MiltonGuiFlags_SHOWING_PREVIEW);
}

Rect get_bounds_for_picker_and_colors(ColorPicker* picker)
{
    Rect result = rect_union(picker_get_bounds(picker), picker_color_buttons_bounds(picker));
    return result;
}


v3f picker_hsv_from_point(ColorPicker* picker, v2f point)
{
    float area = orientation(picker->info.a, picker->info.b, picker->info.c);
    v3f hsv = {0};
    if (area != 0) {
        float inv_area = 1.0f / area;
        float s = orientation(picker->info.b, point, picker->info.a) * inv_area;
        if (s > 1) { s = 1; }
        if (s < 0) { s = 0; }
        float v = 1 - (orientation(point, picker->info.c, picker->info.a) * inv_area);
        if (v > 1) { v = 1; }
        if (v < 0) { v = 0; }

        hsv = (v3f){
            picker->info.hsv.h,
            s,
            v,
        };
    } else {
        int wtf =1;
    }
    return hsv;
}

v3f gui_get_picker_rgb(MiltonGui* gui)
{
    v3f rgb = hsv_to_rgb(gui->picker.info.hsv);
    return rgb;
}

// Returns true if the GUI consumed input. False if the GUI wasn't affected
b32 gui_consume_input(MiltonGui* gui, MiltonInput* input)
{
    b32 accepts = false;
    v2i point = input->points[0];
    if ( gui->visible ) {
        accepts = picker_is_accepting_input(&gui->picker, point);
        if ( !gui->did_hit_button &&
             !picker_is_active(&gui->picker) &&
             picker_hit_history_buttons(&gui->picker, point)) {
            accepts = true;
            gui->did_hit_button = true;
        }
    }
    return accepts;
}

MiltonRenderFlags gui_process_input(MiltonState* milton_state, MiltonInput* input)
{
    MiltonRenderFlags render_flags = MiltonRenderFlags_NONE;
    v2i point = input->points[0];
    ColorPickResult pick_result = picker_update(&milton_state->gui->picker, point);
    if ( pick_result == ColorPickResult_CHANGE_COLOR &&
         milton_state->current_mode == MiltonMode_PEN ) {
        /* v3f rgb = hsv_to_rgb(milton_state->gui->picker.info.hsv); */
        /* milton_state->brushes[BrushEnum_PEN].color = to_premultiplied(rgb, milton_state->brushes[BrushEnum_PEN].alpha); */
    }

    set_flag(render_flags, MiltonRenderFlags_PICKER_UPDATED);
    milton_state->gui->active = true;


    return render_flags;
}

void gui_toggle_visibility(MiltonGui* gui)
{
    gui->visible = !gui->visible;
}

void gui_toggle_help(MiltonGui* gui)
{
    gui->show_help_widget = !gui->show_help_widget;
}

void gui_init(Arena* root_arena, MiltonGui* gui)
{
    i32 bounds_radius_px = 100;
    f32 wheel_half_width = 12;
    gui->picker.center = (v2i){ bounds_radius_px + 20, bounds_radius_px + 30 };
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
    gui->picker.pixels = arena_alloc_array(root_arena, (4 * bounds_radius_px * bounds_radius_px), u32);
    gui->visible = true;

    picker_init(&gui->picker);

    i32 spacing = 4;
    i32 num_buttons = 5;

    i32 button_size = (2*bounds_radius_px - (num_buttons - 1) * spacing) / num_buttons;

    i32 current_x = 40 - button_size / 2;

    ColorButton* cur_button = &gui->picker.color_buttons;
    for (i32 i = 0; i < num_buttons; ++i) {
        assert (cur_button->next == NULL);

        cur_button->x = current_x;
        cur_button->y = gui->picker.center.y + bounds_radius_px + spacing;
        cur_button->w = button_size;
        cur_button->h = button_size;
        cur_button->color = (v4f){0.0, 0.0, 0.0, 0.0};

        current_x += spacing + button_size;

        if (i != (num_buttons - 1)) {
            cur_button->next = arena_alloc_elem(root_arena, ColorButton);
        }
        cur_button = cur_button->next;
    }

    gui->preview_pos      = (v2i){-1, -1};
    gui->preview_pos_prev = (v2i){-1, -1};

    exporter_init(&gui->exporter);
}

// When a selected color is used in a stroke, call this to update the color
// button list.
b32 gui_mark_color_used(MiltonGui* gui, v3f stroke_color)
{
    b32 changed = false;
    ColorButton* start = &gui->picker.color_buttons;
    v3f picker_color  = hsv_to_rgb(gui->picker.info.hsv);
    // Search for a color that is already in the list
    ColorButton* button = start;
    while(button) {
        if ( button->color.a != 0) {
            v3f diff = {
                fabsf(button->color.r - picker_color.r),
                fabsf(button->color.g - picker_color.g),
                fabsf(button->color.b - picker_color.b),
            };
            float epsilon = 0.000001f;
            if (diff.r < epsilon && diff.g < epsilon && diff.b < epsilon) {
                // Move this button to the start and return.
                changed = true;
                v4f tmp_color = button->color;
                PickerData tmp_data = button->picker_data;
                button->color = start->color;
                button->picker_data = start->picker_data;
                start->color = tmp_color;
                start->picker_data = tmp_data;
            }
        }
        button = button->next;
    }
    button = start;

    // If not found, add to list.
    if ( !changed ) {
        changed = true;
        v4f button_color = color_rgb_to_rgba(picker_color,1);
        PickerData picker_data = gui->picker.info;
        // Pass info to the next one.
        while ( button ) {
            v4f tmp_color = button->color;
            PickerData tmp_data = button->picker_data;
            button->color = button_color;
            button->picker_data = picker_data;
            button_color = tmp_color;
            picker_data = tmp_data;
            button = button->next;
        }

    }

    return changed;
}

void gui_deactivate(MiltonGui* gui)
{
    picker_deactivate(&gui->picker);

    // Reset transient values
    gui->active = false;
    gui->did_hit_button = false;
}

