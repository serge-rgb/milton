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

#include <imgui.h>

static Rect color_button_as_rect(const ColorButton* button)
{
    Rect rect = {};
    rect.left = button->center_x - button->width;
    rect.right = button->center_x + button->width;
    rect.top = button->center_y - button->height;
    rect.bottom = button->center_y + button->height;
    return rect;
}

static void picker_update_wheel(ColorPicker* picker, v2f polar_point)
{
    float angle = picker_wheel_get_angle(picker, polar_point);
    picker->info.hsv.h = radians_to_degrees(angle);
    // Update the triangle
    {
        float radius = 0.9f * (picker->wheel_radius - picker->wheel_half_width);
        v2f center = v2i_to_v2f(picker->center);
        {
            v2f point = polar_to_cartesian(-angle, radius);
            point = point + center;
            picker->info.c = point;
        }
        {
            v2f point = polar_to_cartesian(-angle + 2 * kPi / 3.0f, radius);
            point = point + center;
            picker->info.b = point;
        }
        {
            v2f point = polar_to_cartesian(-angle + 4 * kPi / 3.0f, radius);
            point = point + center;
            picker->info.a = point;
        }
    }
}

static b32 picker_hits_triangle(ColorPicker* picker, v2f fpoint)
{
    b32 result = is_inside_triangle(fpoint, picker->info.a, picker->info.b, picker->info.c);
    return result;
}

static void picker_deactivate(ColorPicker* picker)
{
    picker->flags = ColorPickerFlags::NOTHING;
}

static b32 is_inside_picker_rect(ColorPicker* picker, v2i point)
{
    return is_inside_rect(picker->bounds, point);
}

static Rect picker_color_buttons_bounds(const ColorPicker* picker)
{
    Rect bounds = {};
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


b32 picker_hits_wheel(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = point - center;
    float dist = magnitude(arrow);

    b32 hits = (dist <= picker->wheel_radius + picker->wheel_half_width ) &&
               (dist >= picker->wheel_radius - picker->wheel_half_width );

    return hits;
}

float picker_wheel_get_angle(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = point - center;
    v2f baseline = { 1, 0 };
    float dotp = (DOT(arrow, baseline)) / (magnitude(arrow));
    float angle = acosf(dotp);
    if (point.y > center.y) {
        angle = (2 * kPi) - angle;
    }
    return angle;
}


b32 is_inside_picker_active_area(ColorPicker* picker, v2i point)
{
    v2f fpoint = v2i_to_v2f(point);
    b32 result = picker_hits_wheel(picker, fpoint) ||
                 is_inside_triangle(fpoint, picker->info.a, picker->info.b, picker->info.c);
    return result;
}

static b32 picker_is_accepting_input(ColorPicker* picker, v2i point)
{
    // If wheel is active, yes! Gimme input.
    if (check_flag(picker->flags, ColorPickerFlags::WHEEL_ACTIVE)) {
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

static ColorPickResult picker_update(ColorPicker* picker, v2i point)
{
    ColorPickResult result = ColorPickResult::NOTHING;
    v2f fpoint = v2i_to_v2f(point);
    if (picker->flags == ColorPickerFlags::NOTHING) {
        if (picker_hits_wheel(picker, fpoint)) {
            set_flag(picker->flags, ColorPickerFlags::WHEEL_ACTIVE);
        }
    }
    if (check_flag(picker->flags, ColorPickerFlags::WHEEL_ACTIVE)) {
        if (!(check_flag(picker->flags, ColorPickerFlags::TRIANGLE_ACTIVE))) {
            picker_update_wheel(picker, fpoint);
            result = ColorPickResult::CHANGE_COLOR;
        }
    }
    if (picker_hits_triangle(picker, fpoint)) {
        if (!(check_flag(picker->flags, ColorPickerFlags::WHEEL_ACTIVE))) {
            set_flag(picker->flags, ColorPickerFlags::TRIANGLE_ACTIVE);
            picker->info.hsv = picker_hsv_from_point(picker, fpoint);
            result = ColorPickResult::CHANGE_COLOR;
        }
    } else if (check_flag(picker->flags, ColorPickerFlags::TRIANGLE_ACTIVE)) {
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
                result = ColorPickResult::CHANGE_COLOR;
                break;
            }
        }
    }

    return result;
}

void milton_gui_tick(MiltonInputFlags& input, MiltonState& milton_state)
{
    // ImGui Section

    const float pen_alpha = milton_get_pen_alpha(milton_state);
    assert(pen_alpha >= 0.0f && pen_alpha <= 1.0f);
    // Spawn below the picker
    Rect pbounds = get_bounds_for_picker_and_colors(milton_state.gui->picker);

    /* ImGuiSetCond_Always        = 1 << 0, // Set the variable */
    /* ImGuiSetCond_Once          = 1 << 1, // Only set the variable on the first call per runtime session */
    ImGui::SetNextWindowPos(ImVec2(10, 10 + (float)pbounds.bottom), ImGuiSetCond_Once);

    int color_stack = 0;
    ImGui::GetStyle().WindowFillAlphaDefault = 0.9f;  // Redundant for all calls but the first one...
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{.5f,.5f,.5f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4{.5f,.5f,.5f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4{.6f,.6f,.6f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{.3f,.3f,.4f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{.1f,.1f,.1f,1}); ++color_stack;
    ImGui::Begin("Brushes");
    {
        if (check_flag(milton_state.current_mode, MiltonMode::PEN)) {
            float mut_alpha = pen_alpha;
            ImGui::SliderFloat("Opacity", &mut_alpha, 0.1f, 1.0f);
            if ( mut_alpha != pen_alpha ) {
                milton_set_pen_alpha(&milton_state, mut_alpha);
                milton_state.gui->is_showing_preview = true;
            }
        }

        assert (check_flag(milton_state.current_mode, MiltonMode::ERASER) ||
                check_flag(milton_state.current_mode, MiltonMode::PEN));

        const auto size = milton_get_brush_size(milton_state);
        auto mut_size = size;

        ImGui::SliderInt("Brush Size", &mut_size, 1, k_max_brush_size);

        if (mut_size != size) {
            milton_set_brush_size(milton_state, mut_size);
            milton_state.gui->is_showing_preview = true;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1,1,1,1});
        {
            if (!check_flag(milton_state.current_mode, MiltonMode::PEN)) {
                if (ImGui::Button("Switch to Pen")) {
                    set_flag(input, MiltonInputFlags::SET_MODE_PEN);
                }
            }

            if (!check_flag(milton_state.current_mode, MiltonMode::ERASER)) {
                if (ImGui::Button("Switch to Eraser")) {
                    set_flag(input, MiltonInputFlags::SET_MODE_ERASER);
                }
            }
        }
        ImGui::PopStyleColor(1); // Pop white button text

        //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    }
    const v2i pos = {
        (i32)(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x + milton_get_brush_size(milton_state)),
        (i32)(ImGui::GetWindowPos().y),
    };
    ImGui::End();
    ImGui::PopStyleColor(color_stack);

    if ( milton_state.gui->is_showing_preview ) {
        milton_set_brush_preview(milton_state, pos);
    }

}

static void picker_init(ColorPicker* picker)
{
    v2f fpoint = {
        (f32)picker->center.x + (int)(picker->wheel_radius),
        (f32)picker->center.y
    };
    picker_update_wheel(picker, fpoint);
    picker->info.hsv = { 0, 1, 1 };
}

static b32 picker_is_active(ColorPicker* picker)
{
    b32 is_active = check_flag(picker->flags, ColorPickerFlags::WHEEL_ACTIVE) ||
            check_flag(picker->flags, ColorPickerFlags::TRIANGLE_ACTIVE);

    return is_active;
}

static Rect picker_get_bounds(const ColorPicker* picker)
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

void gui_imgui_ungrabbed(MiltonGui& gui)
{
    gui.is_showing_preview = false;
}

Rect get_bounds_for_picker_and_colors(const ColorPicker& picker)
{
    Rect result = rect_union(picker_get_bounds(&picker), picker_color_buttons_bounds(&picker));
    return result;
}


v3f picker_hsv_from_point(ColorPicker* picker, v2f point)
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

v3f gui_get_picker_rgb(MiltonGui* gui)
{
    v3f rgb = hsv_to_rgb(gui->picker.info.hsv);
    return rgb;
}

// Returns true if the GUI consumed input. False if the GUI wasn't affected
b32 gui_consume_input(MiltonGui* gui, MiltonInput* input)
{
    v2i point = input->points[0];
    b32 accepts = picker_is_accepting_input(&gui->picker, point);
    if ( !gui->did_hit_button &&
         !picker_is_active(&gui->picker) &&
         picker_hit_history_buttons(&gui->picker, point)) {
        accepts = true;
        gui->did_hit_button = true;
    }
    return accepts;
}

MiltonRenderFlags gui_process_input(MiltonState* milton_state, MiltonInput* input)
{
    MiltonRenderFlags render_flags = MiltonRenderFlags::NONE;
    v2i point = input->points[0];
    ColorPickResult pick_result = picker_update(&milton_state->gui->picker, point);
    if ( (pick_result == ColorPickResult::CHANGE_COLOR) &&
         check_flag(milton_state->current_mode, MiltonMode::PEN) ) {
        milton_state->gui->did_change_color = true;
        v3f rgb = hsv_to_rgb(milton_state->gui->picker.info.hsv);
        milton_state->brushes[BrushEnum_PEN].color =
                to_premultiplied(rgb, milton_state->brushes[BrushEnum_PEN].alpha);
    }
    set_flag(render_flags, MiltonRenderFlags::PICKER_UPDATED);
    milton_state->gui->active = true;


    return render_flags;
}

void gui_init(Arena* root_arena, MiltonGui* gui)
{
    i32 bounds_radius_px = 100;
    f32 wheel_half_width = 12;
    gui->picker.center = { bounds_radius_px + 20, bounds_radius_px + 20 };
    gui->picker.bounds_radius_px = bounds_radius_px;
    gui->picker.wheel_half_width = wheel_half_width;
    gui->picker.wheel_radius = (f32)bounds_radius_px - 5.0f - wheel_half_width;
    gui->picker.info.hsv = { 0.0f, 1.0f, 0.7f };
    Rect bounds;
    bounds.left = gui->picker.center.x - bounds_radius_px;
    bounds.right = gui->picker.center.x + bounds_radius_px;
    bounds.top = gui->picker.center.y - bounds_radius_px;
    bounds.bottom = gui->picker.center.y + bounds_radius_px;
    gui->picker.bounds = bounds;
    gui->picker.pixels = arena_alloc_array(root_arena, (4 * bounds_radius_px * bounds_radius_px), u32);

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
        cur_button->color = {0.0, 0.0, 0.0, 0.0};

        current_center_x += spacing + button_size;

        if (i != (num_buttons - 1)) {
            cur_button->next = arena_alloc_elem(root_arena, ColorButton);
        }
        cur_button = cur_button->next;
    }

    gui->preview_pos      = {-1, -1};
    gui->preview_pos_prev = {-1, -1};
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
            float epsilon = 0.000001;
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
    gui->did_change_color = false;
    gui->did_hit_button = false;
}
