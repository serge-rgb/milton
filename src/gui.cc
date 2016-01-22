// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.

#include <imgui.h>

static Rect color_button_as_rect(const ColorButton* button)
{
    Rect rect = {};
    rect.left = button->x;
    rect.right = button->x + button->w;
    rect.top = button->y;
    rect.bottom = button->y + button->h;
    return rect;
}

static void picker_update_wheel(ColorPicker* picker, v2f polar_point)
{
    float angle = picker_wheel_get_angle(picker, polar_point);
    picker->info.hsv.h = radians_to_degrees(angle);
    // Update the triangle
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

static b32 is_inside_picker_active_area(ColorPicker* picker, v2i point)
{
    v2f fpoint = v2i_to_v2f(point);
    b32 result = picker_hits_wheel(picker, fpoint) ||
                 is_inside_triangle(fpoint, picker->info.a, picker->info.b, picker->info.c);
    return result;
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


static void exporter_init(Exporter* exporter)
{
    *exporter = {};
    exporter->scale = 1;
}

void exporter_input(Exporter* exporter, MiltonInput* input)
{
    if ( input->input_count > 0 ) {
        v2i point = input->points[input->input_count - 1];
        if ( exporter->state == ExporterState::EMPTY ||
             exporter->state == ExporterState::SELECTED ) {
            exporter->pivot = point;
            exporter->needle = point;
            exporter->state = ExporterState::GROWING_RECT;
        } else if ( exporter->state == ExporterState::GROWING_RECT) {
            exporter->needle = point;
        }
    }
    if ( check_flag(input->flags, MiltonInputFlags::END_STROKE) && exporter->state != ExporterState::EMPTY ) {
        exporter->state = ExporterState::SELECTED;
    }
}

void milton_gui_tick(MiltonInput* input, MiltonState* milton_state)
{
    // ImGui Section
    auto default_imgui_window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

    const float pen_alpha = milton_get_pen_alpha(milton_state);
    assert(pen_alpha >= 0.0f && pen_alpha <= 1.0f);
    // Spawn below the picker
    Rect pbounds = get_bounds_for_picker_and_colors(milton_state->gui->picker);

    int color_stack = 0;
    ImGui::GetStyle().WindowFillAlphaDefault = 0.9f;  // Redundant for all calls but the first one...
    ImGui::PushStyleColor(ImGuiCol_WindowBg,        ImVec4{.5f,.5f,.5f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_TitleBg,         ImVec4{.3f,.3f,.3f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,   ImVec4{.4f,.4f,.4f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_Button,          ImVec4{.3f,.3f,.4f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_Text,            ImVec4{1, 1, 1, 1}); ++color_stack;

    ImGui::PushStyleColor(ImGuiCol_MenuBarBg,   ImVec4{.3f,.3f,.3f,1}); ++color_stack;

    //ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{.1f,.1f,.1f,1}); ++color_stack;


    int menu_style_stack = 0;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,        ImVec4{.3f,.3f,.3f,1}); ++menu_style_stack;
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,   ImVec4{.9f,.3f,.3f,1}); ++menu_style_stack;
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   ImVec4{.3f,.3f,.6f,1}); ++menu_style_stack;
    if ( ImGui::BeginMainMenuBar() ) {
        if ( ImGui::BeginMenu("File") ) {
            if ( ImGui::MenuItem("Open Milton Canvas") ) {

            }
            if ( ImGui::MenuItem("Export to image...") ) {
                milton_switch_mode(milton_state, MiltonMode::EXPORTING);
            }
            if ( ImGui::MenuItem("Quit") ) {
                milton_try_quit(milton_state);
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu("Canvas", /*enabled=*/true) ) {
            if ( ImGui::MenuItem("Set Background Color") ) {
                set_flag(milton_state->gui->flags, MiltonGuiFlags::CHOOSING_BG_COLOR);
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu("Help") ) {
            const char* help_window_title = milton_state->gui->show_help_widget ? "Hide Keyboard Shortcuts" : "Show Keyboard Shortcuts";
            if ( ImGui::MenuItem(help_window_title) ) {
                gui_toggle_help(milton_state->gui);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleColor(menu_style_stack);

    // TODO (IMPORTANT): Add a "reset UI" option? widgets might get outside the viewport without a way to get back.


    if ( milton_state->gui->visible ) {

        /* ImGuiSetCond_Always        = 1 << 0, // Set the variable */
        /* ImGuiSetCond_Once          = 1 << 1, // Only set the variable on the first call per runtime session */
        /* ImGuiSetCond_FirstUseEver */

        ImGui::SetNextWindowPos(ImVec2(10, 10 + (float)pbounds.bottom), ImGuiSetCond_Always);
        ImGui::SetNextWindowSize({271, 109}, ImGuiSetCond_Always);  // We don't want to set it *every* time, the user might have preferences

        b32 show_brush_window = (milton_state->current_mode == MiltonMode::PEN || milton_state->current_mode == MiltonMode::ERASER);

        if ( show_brush_window) {
            if ( ImGui::Begin("Brushes", NULL, default_imgui_window_flags) ) {
                if ( milton_state->current_mode == MiltonMode::PEN ) {
                    float mut_alpha = pen_alpha;
                    ImGui::SliderFloat("Opacity", &mut_alpha, 0.1f, 1.0f);
                    if ( mut_alpha != pen_alpha ) {
                        milton_set_pen_alpha(milton_state, mut_alpha);
                        set_flag(milton_state->gui->flags, MiltonGuiFlags::SHOWING_PREVIEW);
                    }
                }

                const auto size = milton_get_brush_size(milton_state);
                auto mut_size = size;

                ImGui::SliderInt("Brush Size", &mut_size, 1, k_max_brush_size);

                if ( mut_size != size ) {
                    milton_set_brush_size(milton_state, mut_size);
                    set_flag(milton_state->gui->flags, MiltonGuiFlags::SHOWING_PREVIEW);
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1,1,1,1});
                {
                    if ( milton_state->current_mode != MiltonMode::PEN ) {
                        if (ImGui::Button("Switch to Pen")) {
                            set_flag(input->flags, MiltonInputFlags::CHANGE_MODE);
                            input->mode_to_set = MiltonMode::PEN;
                        }
                    }

                    if ( milton_state->current_mode != MiltonMode::ERASER ) {
                        if (ImGui::Button("Switch to Eraser")) {
                            set_flag(input->flags, MiltonInputFlags::CHANGE_MODE);
                            input->mode_to_set = MiltonMode::ERASER;
                        }
                    }
                }
                ImGui::PopStyleColor(1); // Pop white button text

                // ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            }
            // Important to place this before ImGui::End()
            const v2i pos = {
                (i32)(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x + milton_get_brush_size(milton_state)),
                (i32)(ImGui::GetWindowPos().y),
            };
            ImGui::End();  // Brushes
            if ( check_flag(milton_state->gui->flags, MiltonGuiFlags::SHOWING_PREVIEW) ) {
                milton_state->gui->preview_pos = pos;
            }
        }



        if ( check_flag(milton_state->gui->flags, MiltonGuiFlags::CHOOSING_BG_COLOR) ) {
            bool closed = false;
            if ( ImGui::Begin("Choose Background Color", &closed, default_imgui_window_flags) ) {
                ImGui::SetWindowSize({271, 109}, ImGuiSetCond_Always);
                ImGui::Text("Sup");
                float color[3];
                if ( ImGui::ColorEdit3("Background Color", milton_state->view->background_color.d) ) {
                    set_flag(input->flags, MiltonInputFlags::FULL_REFRESH);
                    set_flag(input->flags, MiltonInputFlags::FAST_DRAW);
                }
                if ( closed ) {
                    unset_flag(milton_state->gui->flags, MiltonGuiFlags::CHOOSING_BG_COLOR);
                }
            }
            ImGui::End();
        }

    } // Visible

    // Note: The export window is drawn regardless of gui visibility.
    if ( milton_state->current_mode == MiltonMode::EXPORTING ) {
        bool closed = false;
        b32 reset = false;

        ImGui::SetNextWindowPos(ImVec2(100, 30), ImGuiSetCond_Once);
        ImGui::SetNextWindowSize({350, 235}, ImGuiSetCond_Always);  // We don't want to set it *every* time, the user might have preferences
        if ( ImGui::Begin("Export...", &closed, ImGuiWindowFlags_NoCollapse) ) {
            ImGui::Text("Click and drag to select the area to export.");

            Exporter* exporter = &milton_state->gui->exporter;
            if ( exporter->state == ExporterState::SELECTED ) {
                i32 x = min( exporter->needle.x, exporter->pivot.x );
                i32 y = min( exporter->needle.y, exporter->pivot.y );
                int raster_w = abs(exporter->needle.x - exporter->pivot.x);
                int raster_h = abs(exporter->needle.y - exporter->pivot.y);

                ImGui::Text("Current selection is %dx%d\n",
                            raster_w, raster_h);
                if ( ImGui::InputInt("Scale up", &exporter->scale, 1, /*step_fast=*/2) ) {}
                if ( exporter->scale <= 0 ) exporter->scale = 1;
                ImGui::Text("Final image size: %dx%d\n", raster_w*exporter->scale, raster_h*exporter->scale);

                if ( ImGui::Button("Export selection to image...") ) {
                    // Render to buffer
                    int bpp = 4;  // bytes per pixel
                    i32 w = raster_w * exporter->scale;
                    i32 h = raster_h * exporter->scale;
                    size_t size = w * h * bpp;
                    u8* buffer = (u8*)mlt_malloc(size);
                    if (buffer) {
                        milton_render_to_buffer(milton_state, buffer, x,y, raster_w, raster_h, exporter->scale);
                        milton_save_buffer_to_file(milton_state, buffer, w, h);
                        mlt_free (buffer);
                    } else {
                        milton_die_gracefully("Buffer too large.");
                    }
                }
            }
        }

        if ( ImGui::Button("Cancel") ) {
            reset = true;
            milton_use_previous_mode(milton_state);
        }
        ImGui::End(); // Export...
        if ( closed ) {
            reset = true;
            milton_use_previous_mode(milton_state);
        }
        if ( reset ) {
            exporter_init(&milton_state->gui->exporter);
        }
    }

    // Shortcut help. Also shown regardless of UI visibility.
    if ( milton_state->gui->show_help_widget ) {
        //bool opened;
        ImGui::SetNextWindowPos(ImVec2(365, 92), ImGuiSetCond_Always);
        ImGui::SetNextWindowSize({235, 235}, ImGuiSetCond_Always);  // We don't want to set it *every* time, the user might have preferences
        bool closed;
        if ( ImGui::Begin("Shortcuts", &closed, default_imgui_window_flags) ) {
            ImGui::TextWrapped(
                               "Increase brush size        ]\n"
                               "Decrease brush size        [\n"
                               "Pen                        b\n"
                               "Eraser                     e\n"
                               "10%%  opacity               1\n"
                               "20%%  opacity               2\n"
                               "30%%  opacity               3\n"
                               "             ...             \n"
                               "90%%  opacity               9\n"
                               "100%% opacity               0\n"
                               "\n"
                               "Show/Hide Help Window     F1\n"
                               "Toggle GUI Visibility     Tab\n"
                               "\n"
                               "\n"
                              );
            if ( closed ) {
                milton_state->gui->show_help_widget = false;
            }
        }
        ImGui::End();  // Help
    }


    ImGui::PopStyleColor(color_stack);

}

void gui_imgui_set_ungrabbed(MiltonGui* gui)
{
    unset_flag(gui->flags, MiltonGuiFlags::SHOWING_PREVIEW);
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
    if ( pick_result == ColorPickResult::CHANGE_COLOR &&
         milton_state->current_mode == MiltonMode::PEN ) {
        milton_state->gui->did_change_color = true;
        v3f rgb = hsv_to_rgb(milton_state->gui->picker.info.hsv);
        milton_state->brushes[BrushEnum_PEN].color =
                to_premultiplied(rgb, milton_state->brushes[BrushEnum_PEN].alpha);
    }

    set_flag(render_flags, MiltonRenderFlags::PICKER_UPDATED);
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
    gui->picker.center = { bounds_radius_px + 20, bounds_radius_px + 30 };
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
        cur_button->color = {0.0, 0.0, 0.0, 0.0};

        current_x += spacing + button_size;

        if (i != (num_buttons - 1)) {
            cur_button->next = arena_alloc_elem(root_arena, ColorButton);
        }
        cur_button = cur_button->next;
    }

    gui->preview_pos      = {-1, -1};
    gui->preview_pos_prev = {-1, -1};

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
