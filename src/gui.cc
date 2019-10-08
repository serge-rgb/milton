// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "gui.h"

#include "localization.h"
#include "color.h"
#include "renderer.h"
#include "milton.h"
#include "persist.h"
#include "platform.h"


#define NUM_BUTTONS 5
#define BOUNDS_RADIUS_PX 80

// If reset_gui is true, the default window position and size will be set.
void
gui_layer_window(MiltonInput* input, PlatformState* platform, Milton* milton, f32 brush_window_height, PlatformSettings* prefs, b32 reset_gui)
{
    float ui_scale = milton->gui->scale;
    MiltonGui* gui = milton->gui;
    const Rect pbounds = get_bounds_for_picker_and_colors(&gui->picker);
    CanvasState* canvas = milton->canvas;

    // Layer window

    // Use default size on first program start on this computer.
    f32 left   = ui_scale*10;
    f32 top    = ui_scale*20 + (float)pbounds.bottom + brush_window_height;
    f32 width  = ui_scale*300;
    f32 height = ui_scale*230;
    if ( reset_gui ) {
        ImGui::SetNextWindowPos(ImVec2(left, top));
        ImGui::SetNextWindowSize(ImVec2(width, height));
    }
    else {
        if ( prefs->layer_window_width != 0 && prefs->layer_window_height != 0 ) {
            // If there are preferences already, use those for the layer window.
            left   = prefs->layer_window_left;
            top    = prefs->layer_window_top;
            width  = prefs->layer_window_width;
            height = prefs->layer_window_height;
        }

        ImGui::SetNextWindowPos(ImVec2(left, top), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiSetCond_FirstUseEver);
    }

    if ( ImGui::Begin(loc(TXT_layers)) ) {
        i32 angle = (milton->view->angle / PI) * 180;
        while (angle < 0) { angle += 360; }
        while (angle > 360) { angle -= 360; }
        if (ImGui::SliderInt(loc(TXT_rotation), &angle, 0.0f, 360)) {
            milton->view->angle = (f32)angle / 180.0f * PI;
            input->flags |= (i32)MiltonInputFlags_PANNING;
            gpu_update_canvas(milton->renderer, milton->canvas, milton->view);
        }

        CanvasView* view = milton->view;
        // left
        ImGui::BeginChild("left pane", ImVec2(150, 0), true);

        static b32 is_renaming = false;
        static i32 layer_renaming_idx = -1;
        static b32 focus_rename_field = false;


        Layer* layer = milton->canvas->root_layer;
        while ( layer->next ) { layer = layer->next; }  // Move to the top layer.
        while ( layer ) {

            bool v = layer->flags & LayerFlags_VISIBLE;
            ImGui::PushID(layer->id);

            if ( ImGui::Checkbox("##select", &v) ) {
                layer::layer_toggle_visibility(layer);
                input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;
            }

            ImGui::PopID();
            ImGui::SameLine();

            // Draw the layers list. If in renaming mode, draw the layer that's being renamed as an InputText.
            // Else just draw them as a list of Selectables.
            if ( !ImGui::IsWindowFocused() ) {
                is_renaming = false;
            }

            if ( is_renaming ) {
                if ( layer->id == layer_renaming_idx ) {
                    if ( focus_rename_field ) {
                        ImGui::SetKeyboardFocusHere(0);
                        focus_rename_field = false;
                    }
                    if ( ImGui::InputText("##rename",
                                          milton->canvas->working_layer->name,
                                          13,
                                          //MAX_LAYER_NAME_LEN,
                                          ImGuiInputTextFlags_EnterReturnsTrue
                                          //,ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL
                                         )) {
                        is_renaming = false;
                    }
                }
                else {
                    if ( ImGui::Selectable(layer->name,
                                           milton->canvas->working_layer == layer,
                                           ImGuiSelectableFlags_AllowDoubleClick) ) {
                        if ( ImGui::IsMouseDoubleClicked(0) ) {
                            layer_renaming_idx = layer->id;
                            focus_rename_field = true;
                        }
                        is_renaming = false;
                        milton_set_working_layer(milton, layer);
                    }
                }
            }
            else {
                if ( ImGui::Selectable(layer->name,
                                       milton->canvas->working_layer == layer,
                                       ImGuiSelectableFlags_AllowDoubleClick) ) {
                    if ( ImGui::IsMouseDoubleClicked(0) ) {
                        layer_renaming_idx = layer->id;
                        is_renaming = true;
                        focus_rename_field = true;
                    }
                    milton_set_working_layer(milton, layer);
                }
            }

            layer = layer->prev;
        }
        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::BeginChild("item view", ImVec2(0, 25));
        if ( ImGui::Button(loc(TXT_new_layer)) ) {
            milton_new_layer(milton);
        }
        ImGui::SameLine();


        ImGui::Separator();
        ImGui::EndChild();
        ImGui::BeginChild("buttons");

        ImGui::Text(loc(TXT_move));

        Layer* a = NULL;
        Layer* b = NULL;
        if ( ImGui::Button(loc(TXT_up)) ) {
            b = milton->canvas->working_layer;
            a = b->next;
        }
        ImGui::SameLine();
        if ( ImGui::Button(loc(TXT_down)) ) {
            a = milton->canvas->working_layer;
            b = a->prev;
        }


        if ( a && b ) {
            // n <-> a <-> b <-> p
            // n <-> b <-> a <-> p
            Layer* n = a->next;
            Layer* p = b->prev;
            b->next = n;
            if ( n ) n->prev = b;
            a->prev = p;
            if ( p ) p->next = a;

            a->next = b;
            b->prev = a;

            // Make sure root is first
            while ( milton->canvas->root_layer->prev ) {
                milton->canvas->root_layer = milton->canvas->root_layer->prev;
            }
            input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;
        }

        if ( milton->canvas->working_layer->next
             || milton->canvas->working_layer->prev ) {
            static bool deleting = false;
            if ( deleting == false ) {
                if ( ImGui::Button(loc(TXT_delete)) ) {
                    deleting = true;
                }
            }
            else if ( deleting ) {
                ImGui::Text(loc(TXT_are_you_sure));
                ImGui::Text(loc(TXT_cant_be_undone));
                if ( ImGui::Button(loc(TXT_yes)) ) {
                    milton_delete_working_layer(milton);
                    input->flags |= MiltonInputFlags_FULL_REFRESH;
                    deleting = false;
                }
                ImGui::SameLine();
                if ( ImGui::Button(loc(TXT_no)) ) {
                    deleting = false;
                }
            }
        }
        static b32 show_effects = false;
        {
            // ImGui::GetWindow(Pos|Size) works in here because we are inside Begin()/End() calls.
            {
                ImGui::Text(loc(TXT_opacity));
                f32 alpha = canvas->working_layer->alpha;
                if ( ImGui::SliderFloat("##opacity", &alpha, 0.0f, 1.0f) ) {
                    // Used the slider. Ask if it's OK to convert the binary format.
                    if ( milton->persist->mlt_binary_version < 3 ) {
                        milton_log("Modified milton file from %d to 3\n", milton->persist->mlt_binary_version);
                        milton->persist->mlt_binary_version = 3;
                    }
                    input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;

                    alpha = clamp(alpha, 0.0f, 1.0f);

                    canvas->working_layer->alpha = alpha;
                }

                static b32 selecting = false;

                ImGui::Separator();

                if ( ImGui::Button(loc(TXT_blur)) ) {
                    LayerEffect* e = arena_alloc_elem(&milton->canvas_arena, LayerEffect);
                    e->next = milton->canvas->working_layer->effects;
                    milton->canvas->working_layer->effects = e;
                    e->enabled = true;
                    e->blur.original_scale = milton->view->scale;
                    e->blur.kernel_size = 10;
                    input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;
                }

                LayerEffect* prev = NULL;
                int effect_id = 1;
                for ( LayerEffect* e = milton->canvas->working_layer->effects; e != NULL; e = e->next ) {
                    ImGui::PushID(effect_id);
                    static bool v = 0;
                    if ( ImGui::Checkbox(loc(TXT_enabled), (bool*)&e->enabled) ) {
                        input->flags |= MiltonInputFlags_FULL_REFRESH;
                    }
                    if ( ImGui::SliderInt(loc(TXT_level), &e->blur.kernel_size, 2, 100, 0) ) {
                        if (e->blur.kernel_size % 2 == 0) {
                            --e->blur.kernel_size;
                        }
                        input->flags |= MiltonInputFlags_FULL_REFRESH;
                    }
                    {
                        if (ImGui::Button(loc(TXT_delete_blur))) {
                            if (prev) {
                                prev->next = e->next;
                            } else {  // Was the first.
                                milton->canvas->working_layer->effects = e->next;
                            }
                            input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;
                        }
                    }
                    ImGui::PopID();
                    prev = e;
                    ImGui::Separator();
                    effect_id++;
                }
                // ImGui::Slider
            }
        }
        ImGui::EndChild();
        ImGui::EndGroup();

        // Remember the current window layout for the next time the program runs.
        ImVec2 pos  = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        prefs->layer_window_left   = pos.x;
        prefs->layer_window_top    = pos.y;
        prefs->layer_window_width  = size.x;
        prefs->layer_window_height = size.y;

    } ImGui::End();
}


// gui_brush_window returns the height of the rendered brush tool window. This can be used to position other windows below it.
// If reset_gui is true, the default window position and size will be set.
i32
gui_brush_window(MiltonInput* input, PlatformState* platform, Milton* milton, PlatformSettings* prefs, b32 reset_gui)
{
    b32 show_brush_window = (current_mode_is_for_drawing(milton));
    auto imgui_window_flags = ImGuiWindowFlags_NoCollapse;
    MiltonGui* gui = milton->gui;

    const Rect pbounds = get_bounds_for_picker_and_colors(&gui->picker);

    // Use default size on first program start on this computer.
    f32 left = milton->gui->scale * 10;
    f32 top = milton->gui->scale * 10 + (float)pbounds.bottom;
    f32 width = milton->gui->scale * 300;
    f32 height = milton->gui->scale * 230;
    if ( reset_gui ) {
        ImGui::SetNextWindowPos(ImVec2(left, top));
        ImGui::SetNextWindowSize({width, height});
    }
    else {
        if ( prefs->brush_window_width != 0 && prefs->brush_window_height ) {
            // If there are preferences already, use those for the layer window.
            left =   prefs->brush_window_left;
            top =    prefs->brush_window_top;
            width =  prefs->brush_window_width;
            height = prefs->brush_window_height;
        }

        ImGui::SetNextWindowPos(ImVec2(left, top), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize({width, height}, ImGuiSetCond_FirstUseEver);
    }

    // Brush Window
    if ( show_brush_window ) {
        if ( ImGui::Begin(loc(TXT_brushes), NULL, imgui_window_flags) ) {
            if ( milton->current_mode == MiltonMode::PEN ||
                 milton->current_mode == MiltonMode::PRIMITIVE ) {
                const float pen_alpha = milton_get_brush_alpha(milton);
                mlt_assert(pen_alpha >= 0.0f && pen_alpha <= 1.0f);
                float mut_alpha = pen_alpha*100;
                ImGui::SliderFloat(loc(TXT_opacity), &mut_alpha, 1, 100, "%.0f%%");

                mut_alpha /= 100.0f;
                if (mut_alpha > 1.0f ) mut_alpha = 1.0f;
                if ( mut_alpha != pen_alpha ) {
                    milton_set_brush_alpha(milton, mut_alpha);
                    gui->flags |= (i32)MiltonGuiFlags_SHOWING_PREVIEW;
                }
            }
            const auto size = milton_get_brush_radius(milton);
            auto mut_size = size;


            if (ImGui::CheckboxFlags(loc(TXT_size_relative_to_canvas),
                                    reinterpret_cast<u32*>(&milton->working_stroke.flags),
                                    StrokeFlag_RELATIVE_TO_CANVAS)) {
                // Just set it to be relative...
            }

            ImGui::SliderInt(loc(TXT_brush_size), &mut_size, 1, MILTON_MAX_BRUSH_SIZE);

            if ( mut_size != size ) {
                milton_set_brush_size(milton, mut_size);
                milton->gui->flags |= (i32)MiltonGuiFlags_SHOWING_PREVIEW;
            }

            if ( milton->current_mode != MiltonMode::PEN ) {
                if ( ImGui::Button(loc(TXT_switch_to_brush)) ) {
                    input->mode_to_set = MiltonMode::PEN;
                }
            }

            if ( milton->current_mode != MiltonMode::PRIMITIVE ) {
                if ( ImGui::Button(loc(TXT_switch_to_primitive)) ) {
                    input->mode_to_set = MiltonMode::PRIMITIVE;
                }
            }

            if ( milton->current_mode != MiltonMode::ERASER ) {
                if ( ImGui::Button(loc(TXT_switch_to_eraser)) ) {
                    input->mode_to_set = MiltonMode::ERASER;
                }
            }
        }

        {
            if (!(milton->working_stroke.flags & StrokeFlag_ERASER)) {
                ImGui::CheckboxFlags(loc(TXT_opacity_pressure), reinterpret_cast<u32*>(&milton->working_stroke.flags), StrokeFlag_PRESSURE_TO_OPACITY);
                if (milton->working_stroke.flags & StrokeFlag_PRESSURE_TO_OPACITY) {
                    int brush_enum = milton_get_brush_enum(milton);
                    f32* min_opacity = &milton->brushes[brush_enum].pressure_opacity_min;

                    ImGui::SliderFloat(loc(TXT_minimum), min_opacity, 0.0f, milton->brushes[brush_enum].alpha);
                }
                ImGui::CheckboxFlags(loc(TXT_soft_brush), reinterpret_cast<u32*>(&milton->working_stroke.flags), StrokeFlag_DISTANCE_TO_OPACITY);
                if (milton->working_stroke.flags & StrokeFlag_DISTANCE_TO_OPACITY) {
                    int brush_enum = milton_get_brush_enum(milton);
                    f32* hardness = &milton->brushes[brush_enum].hardness;

                    ImGui::SliderFloat(loc(TXT_hardness), hardness, 1.0f, k_max_hardness);
                }

            }
        }

        // Important to place this before ImGui::End(), position and size will be invalid after it.
        ImVec2 pos  = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        // Remember the current window layout for the next time the program runs.
        prefs->brush_window_left   = pos.x;
        prefs->brush_window_top    = pos.y;
        prefs->brush_window_width  = size.x;
        prefs->brush_window_height = size.y;
        ImGui::End();  // Brushes
        if (( milton->gui->flags & MiltonGuiFlags_SHOWING_PREVIEW )) {
            milton->gui->preview_pos = {
                (i32)(pos.x + size.x + milton_get_brush_radius(milton)),
                (i32)(pos.y),
            };
        }
    }

    return height;
}

void
gui_menu(MiltonInput* input, PlatformState* platform, Milton* milton, b32& show_settings, b32* reset_gui)
{
    // Menu ----
    int menu_style_stack = 0;

    MiltonGui* gui = milton->gui;
    CanvasState* canvas = milton->canvas;

    // TODO: translate

    if ( gui->menu_visible ) {
        if ( ImGui::BeginMainMenuBar() ) {
            if ( ImGui::BeginMenu(loc(TXT_file)) ) {
                if ( ImGui::MenuItem(loc(TXT_new_milton_canvas)) ) {
                    b32 save_file = false;
                    if ( layer::count_strokes(milton->canvas->root_layer) > 0 ) {
                        if ( milton->flags & MiltonStateFlags_DEFAULT_CANVAS ) {
                            save_file = platform_dialog_yesno(loc(TXT_default_will_be_cleared), "Save?");
                        }
                    }
                    if ( save_file ) {
                        PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                        if ( name ) {
                            milton_log("Saving to %s\n", name);
                            milton_set_canvas_file(milton, name);
                            milton_save(milton);
                            b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"), DeleteErrorTolerance_OK_NOT_EXIST);
                            if ( del == false ) {
                                platform_dialog("Could not delete contents. The work will be still be there even though you saved it to a file.",
                                                "Info");
                            }
                        }
                    }

                    // New Canvas
                    milton_reset_canvas_and_set_default(milton);
                    canvas = milton->canvas;
                    input->flags |= MiltonInputFlags_FULL_REFRESH;
                    milton->flags |= MiltonStateFlags_DEFAULT_CANVAS;
                }
                b32 save_requested = false;
                if ( ImGui::MenuItem(loc(TXT_open_milton_canvas)) ) {
                    // If current canvas is MiltonPersist, then prompt to save
                    if ( ( milton->flags & MiltonStateFlags_DEFAULT_CANVAS ) ) {
                        b32 save_file = false;
                        if ( layer::count_strokes(milton->canvas->root_layer) > 0 ) {
                            save_file = platform_dialog_yesno(loc(TXT_default_will_be_cleared), "Save?");
                        }
                        if ( save_file ) {
                            PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                            if ( name ) {
                                milton_log("Saving to %s\n", name);
                                milton_set_canvas_file(milton, name);
                                milton_save(milton);
                                b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"),
                                                                         DeleteErrorTolerance_OK_NOT_EXIST);
                                if ( del == false ) {
                                    platform_dialog("Could not delete default canvas. Contents will be still there when you create a new canvas.",
                                                    "Info");
                                }
                            }
                        }
                    }
                    PATH_CHAR* fname = platform_open_dialog(FileKind_MILTON_CANVAS);
                    if ( fname ) {
                        milton_set_canvas_file(milton, fname);
                        input->flags |= MiltonInputFlags_OPEN_FILE;
                    }
                }
                if ( ImGui::MenuItem(loc(TXT_save_milton_canvas_as_DOTS)) || save_requested ) {
                    // NOTE(possible refactor): There is a copy of this at milton.c end of file
                    PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                    if ( name ) {
                        milton_log("Saving to %s\n", name);
                        milton_set_canvas_file(milton, name);
                        input->flags |= MiltonInputFlags_SAVE_FILE;
                        b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"),
                                                                 DeleteErrorTolerance_OK_NOT_EXIST);
                        if ( del == false ) {
                            platform_dialog(loc(TXT_could_not_delete_default_canvas),
                                            "Info");
                        }
                    }
                }
                if ( ImGui::MenuItem(loc(TXT_export_to_image_DOTS)) ) {
                    milton_enter_mode(milton, MiltonMode::EXPORTING);
                }
                if ( ImGui::MenuItem(loc(TXT_settings)) && !show_settings ) {
                    milton_set_gui_visibility(milton, true);
                    show_settings = true;
                    *gui->original_settings = *milton->settings;
                    for (sz i = Action_FIRST; i < Action_COUNT; ++i) {
                        gui->scratch_binding_key[i][0] = gui->original_settings->bindings.bindings[i].bound_key;
                    }
                }
                if ( ImGui::MenuItem(loc(TXT_quit)) ) {
                    milton_try_quit(milton);
                }
                ImGui::EndMenu();
            } // file menu
            if ( ImGui::BeginMenu(loc(TXT_edit)) ) {
                if ( ImGui::MenuItem(loc(TXT_undo) ) ) {
                    input->flags |= MiltonInputFlags_UNDO;
                }
                if ( ImGui::MenuItem(loc(TXT_redo)) ) {
                    input->flags |= MiltonInputFlags_REDO;
                }
                ImGui::EndMenu();
            }

            if ( ImGui::BeginMenu(loc(TXT_canvas), /*enabled=*/true) ) {
                if ( ImGui::BeginMenu(loc(TXT_set_background_color)) ) {
                    v3f bg = milton->view->background_color;
                    if (ImGui::ColorEdit3(loc(TXT_color), bg.d))
                    {
                        milton_set_background_color(milton, clamp_01(bg));
                        input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;
                    }
                    ImGui::EndMenu();
                }
                if ( ImGui::MenuItem(loc(TXT_zoom_in)) ) {
                    input->scale++;
                    milton_set_zoom_at_screen_center(milton);
                }
                if ( ImGui::MenuItem(loc(TXT_zoom_out)) ) {
                    input->scale--;
                    milton_set_zoom_at_screen_center(milton);
                }
                ImGui::EndMenu();
            }
            if ( ImGui::BeginMenu(loc(TXT_tools)) ) {
                // Brush
                if ( ImGui::MenuItem(loc(TXT_brush)) ) {
                    input->mode_to_set = MiltonMode::PEN;
                }
                if ( ImGui::BeginMenu(loc(TXT_brush_options)) ) {

                    // Toggling brush smoothing is barely noticeable...

                    // b32 smoothing_enabled = milton_brush_smoothing_enabled(milton);
                    // char* entry_str = smoothing_enabled? loc(TXT_disable_stroke_smoothing) : loc(TXT_enable_stroke_smoothing);

                    // if ( ImGui::MenuItem(entry_str) ) {
                    //     milton_toggle_brush_smoothing(milton);
                    // }

                    // Decrease / increase brush size
                    if ( ImGui::MenuItem(loc(TXT_decrease_brush_size)) ) {
                        for (int i=0;i<5;++i) milton_decrease_brush_size(milton);
                    }
                    if ( ImGui::MenuItem(loc(TXT_increase_brush_size)) ) {
                        for (int i=0;i<5;++i) milton_increase_brush_size(milton);
                    }
                    // Opacity shortcuts

                    f32 opacities[] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
                    for ( i32 i = 0; i < array_count(opacities); ++i ) {
                        char entry[128] = {};
                        snprintf(entry, array_count(entry), "%s %d%% - [%d]",
                                 loc(TXT_set_opacity_to), (int)(100 * opacities[i]), i == 9 ? 0 : i+1);
                        if ( ImGui::MenuItem(entry) ) {
                            milton_set_brush_alpha(milton, opacities[i]);
                        }
                    }
                    ImGui::EndMenu();
                }
                // Eraser
                if ( ImGui::MenuItem(loc(TXT_eraser)) ) {
                    input->mode_to_set = MiltonMode::ERASER;
                }
                // Panning
                char* move_str = platform->is_panning==false? loc(TXT_move_canvas) : loc(TXT_stop_moving_canvas);
                if ( ImGui::MenuItem(move_str) ) {
                    platform->waiting_for_pan_input = true;
                }
                // Eye Dropper
                if ( ImGui::MenuItem(loc(TXT_eye_dropper)) ) {
                    input->mode_to_set = MiltonMode::EYEDROPPER;
                }
                ImGui::EndMenu();
            }
            if ( ImGui::BeginMenu(loc(TXT_view)) ) {
                if ( ImGui::MenuItem(loc(TXT_toggle_gui_visibility)) ) {
                    milton_toggle_gui_visibility(milton);
                }

                if ( ImGui::MenuItem(loc(TXT_peek_out)) ) {
                    peek_out_trigger_start(milton, PeekOut_CLICK_TO_EXIT);
                }

                if ( ImGui::MenuItem(loc(TXT_reset_view_at_origin)) ) {
                    reset_transform_at_origin(
                        &milton->view->pan_center,
                        &milton->view->scale,
                        &milton->view->angle);
                    gpu_update_canvas(milton->renderer, milton->canvas, milton->view);
                }

                if ( ImGui::MenuItem(loc(TXT_reset_GUI)) ) {
                    *reset_gui = true;
                }

#if MILTON_ENABLE_PROFILING
                if ( ImGui::MenuItem("Toggle Debug Data [BACKQUOTE]") ) {
                    milton->viz_window_visible = !milton->viz_window_visible;
                }
#endif
                ImGui::EndMenu();
            }
            if ( ImGui::BeginMenu(loc(TXT_help)) ) {
                if ( ImGui::MenuItem(loc(TXT_help_me)) ) {
                    //platform_open_link("https://www.youtube.com/watch?v=g27gHio2Ohk");
                    platform_open_link("http://www.miltonpaint.com/help/");
                }
                if ( ImGui::MenuItem(loc(TXT_milton_version)) ) {
                    char buffer[1024];
                    snprintf(buffer, array_count(buffer),
                             "Milton version %d.%d.%d",
                             MILTON_MAJOR_VERSION, MILTON_MINOR_VERSION, MILTON_MICRO_VERSION);
                    platform_dialog(buffer, "Milton Version");
                }
                if (  ImGui::MenuItem(loc(TXT_website))  ) {
                    platform_open_link("http://miltonpaint.com");
                }
                ImGui::EndMenu();
            }
            PATH_CHAR* utf16_name = str_trim_to_last_slash(milton->persist->mlt_file_path);

            char file_name[MAX_PATH] = {};
            utf16_to_utf8_simple(utf16_name, file_name);

            char msg[1024];
            WallTime lst = milton->persist->last_save_time;

            // TODO: Translate!
            snprintf(msg, 1024, "\t%s -- Last saved: %.2d:%.2d:%.2d\t\tZoom level %.2f",
                     (milton->flags & MiltonStateFlags_DEFAULT_CANVAS) ? loc(TXT_OPENBRACKET_default_canvas_CLOSE_BRACKET):
                     file_name,
                     lst.hours, lst.minutes, lst.seconds,
                     // We divide by MILTON_DEFAULT_SCALE to give a frame of
                     // reference to the user, where 1.0 is the default. For
                     // our calculations in other places, we don't do the
                     // divide.
                     log2(1 + milton->view->scale / (double)MILTON_DEFAULT_SCALE) / log2(SCALE_FACTOR));

            if ( ImGui::BeginMenu(msg, /*bool enabled = */false) ) {
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleColor(menu_style_stack);
    }
}


void
milton_imgui_tick(MiltonInput* input, PlatformState* platform,  Milton* milton, PlatformSettings* prefs)
{
    CanvasState* canvas = milton->canvas;
    MiltonGui* gui = milton->gui;
    // ImGui Section

    // Spawn below the picker
    const Rect pbounds = get_bounds_for_picker_and_colors(&gui->picker);

    int color_stack = 0;

    static auto color_window_background = ImVec4{.929f, .949f, .957f, 1};
    //static auto color_title_bg        = ImVec4{.957f,.353f, .286f,1};
    static auto color_title_bg          = color_window_background;
    static auto color_title_fg          = ImVec4{151/255.f, 184/255.f, 210/255.f, 1};

    static auto color_buttons         = ImVec4{.686f, .796f, 1.0f, 1};
    static auto color_buttons_active  = ImVec4{.886f, .796f, 1.0f, 1};
    static auto color_buttons_hovered = ImVec4{.706f, .816f, 1.0f, 1};

    static auto color_menu_bg        = ImVec4{.784f, .392f, .784f, 1};
    static auto color_text           = ImVec4{.2f,.2f,.2f,1};
    static auto color_slider         = ImVec4{ 148/255.f, 182/255.f, 182/255.f,1};
    static auto frame_background     = ImVec4{ 0.862745f, 0.862745f, 0.862745f,1};
    static auto color_text_selected  = ImVec4{ 0.509804f, 0.627451f, 0.823529f,1};
    static auto color_header_hovered = color_buttons;

    // Helper Imgui code to select color scheme
#if 0
    ImGui::ColorEdit3("Window Background", (float*)&color_window_background);
    ImGui::ColorEdit3("Title background", (float*)&color_title_bg);
    ImGui::ColorEdit3("Title background active", (float*)&color_title_fg);

    ImGui::ColorEdit3("Buttons", (float*)&color_buttons);
    ImGui::ColorEdit3("Menu BG", (float*)&color_menu_bg);
    ImGui::ColorEdit3("text", (float*)&color_text);
    ImGui::ColorEdit3("frame background", (float*)&frame_background);
    ImGui::ColorEdit3("selected", (float*)&color_text_selected);
    if ( ImGui::Button("Print out") ) {
        auto print_color = [&](char* label, ImVec4 c) {
            milton_log("%s : %f, %f, %f \n", label, c.x, c.y, c.z);
        };
        print_color("window bg", color_window_background);
        print_color("title bg", color_title_bg);
        print_color("buttons", color_buttons);
        print_color("menu bg", color_menu_bg);
        print_color("text", color_text);
        print_color("selected", frame_background);
        print_color("selected", color_text_selected);
    }
#endif

    ImGui::PushStyleColor(ImGuiCol_WindowBg,        color_window_background); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_PopupBg,         color_window_background); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_TitleBg,         color_title_bg); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,   color_title_fg); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_Text,            color_text); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg,       color_title_bg); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg,  color_buttons_active); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_FrameBg,      frame_background); ++color_stack;

    ImGui::PushStyleColor(ImGuiCol_SliderGrab,      color_buttons); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,      color_buttons_active); ++color_stack;

    ImGui::PushStyleColor(ImGuiCol_Button,          color_buttons); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,      color_buttons_hovered); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,          color_buttons_active); ++color_stack;

    ImGui::PushStyleColor(ImGuiCol_Header,          color_buttons); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   color_buttons_hovered); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,    color_buttons_active); ++color_stack;

    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,   color_buttons); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,      color_buttons_hovered); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,      color_buttons_active); ++color_stack;

    ImGui::PushStyleColor(ImGuiCol_CheckMark,      color_slider); ++color_stack;


    static b32 show_settings = false;
    b32 reset_gui = false;

    gui_menu(input, platform, milton, show_settings, &reset_gui);

    float ui_scale = milton->gui->scale;

    // GUI Windows ----

    b32 should_show_windows = milton->current_mode != MiltonMode::EYEDROPPER
                                && milton->current_mode != MiltonMode::EXPORTING
                                && milton->current_mode != MiltonMode::HISTORY;

    if ( gui->visible && should_show_windows ) {
        i32 brush_window_height = gui_brush_window(input, platform, milton, prefs, reset_gui);

        gui_layer_window(input, platform, milton, brush_window_height, prefs, reset_gui);

        // Settings window
        if ( show_settings ) {
            ImGui::SetNextWindowSize(ImVec2(ui_scale*400, ui_scale*400),
                                     ImGuiSetCond_FirstUseEver);
            if ( ImGui::Begin(loc(TXT_settings)) ) {
                if (ImGui::Button(loc(TXT_ok))) {
                    milton_settings_save(milton->settings);
                    show_settings = false;
                }
                
                ImGui::SameLine();
                if (ImGui::Button(loc(TXT_cancel))) {
                    show_settings = false;
                    *milton->settings = *gui->original_settings;
                }
                
                ImGui::Separator();
                ImGui::BeginChild("ScrollRegion");

                ImGui::Text(loc(TXT_default_background_color));

                v3f* bg = &milton->settings->background_color;
                if (ImGui::ColorEdit3(loc(TXT_color), bg->d)) {
                    // TODO: Let milton know that we need to save the settings
                }

                if ( ImGui::Button(loc(TXT_set_current_background_color_as_default)) ) {
                    milton->settings->background_color = milton->view->background_color;
                }

                const float peek_range = 20;
                int peek_out_percent = 100 * (milton->settings->peek_out_increment / peek_range);
                if (ImGui::SliderInt(loc(TXT_peek_out_increment_percent), &peek_out_percent, 0, 100)) {
                    milton->settings->peek_out_increment = (peek_out_percent / 100.0f) * peek_range;
                }

                ImGui::Separator();

                MiltonBindings* bs = &milton->settings->bindings;

                for (sz i = Action_FIRST; i < Action_COUNT; ++i ) {
                    Binding* b = bs->bindings + i;

                    char control_lbl[64] = {};
                    snprintf(control_lbl, array_count(control_lbl), "control##%d", (int)i);

                    char alt_lbl[64] = {};
                    snprintf(alt_lbl, array_count(alt_lbl), "alt##%d", (int)i);

                    char win_lbl[64] = {};
                    snprintf(win_lbl, array_count(win_lbl), "win##%d", (int)i);


                    ImGui::CheckboxFlags(control_lbl, (unsigned int*)&b->modifiers, Modifier_CTRL);
                    ImGui::SameLine();
                    ImGui::CheckboxFlags(alt_lbl, (unsigned int*)&b->modifiers, Modifier_ALT);
                    ImGui::SameLine();
                    ImGui::CheckboxFlags(win_lbl, (unsigned int*)&b->modifiers, Modifier_WIN);
                    ImGui::SameLine();

                    ImGui::PushItemWidth(60);
                    // mlt_assert(TXT_Action_FIRST + (int)i < TXT_Count);
                    char* action_str = (char*)loc((Texts)(TXT_Action_FIRST + (int)i - Action_FIRST));
                    if (ImGui::InputText(action_str,
                                         (char*)gui->scratch_binding_key[i],
                                         sizeof(gui->scratch_binding_key[i]),
                                         ImGuiInputTextFlags_AllowTabInput)) {
                        b->bound_key = gui->scratch_binding_key[i][0];
                    }
                    ImGui::PopItemWidth();
                }

                ImGui::EndChild();
            } ImGui::End();
        }

        // History window
        if ( milton->current_mode == MiltonMode::HISTORY ) {
            {
                Rect pb = picker_get_bounds(&gui->picker);
                auto width = 20 + pb.right - pb.left;
                ImGui::SetNextWindowPos(ImVec2(width, 30), ImGuiSetCond_FirstUseEver);
            }
            ImGui::SetNextWindowSize(ImVec2(ui_scale*500, ui_scale*100), ImGuiSetCond_FirstUseEver);
            if ( ImGui::Begin("History Slider") ) {
                ImGui::SliderInt("History", &gui->history, 0,
                                 layer::count_strokes(milton->canvas->root_layer));
            } ImGui::End();

        }
    } // Windows

    // Note: The export window is drawn regardless of gui visibility.
    if ( milton->current_mode == MiltonMode::EXPORTING ) {
        bool opened = true;
        b32 reset = false;

        ImGui::SetNextWindowPos(ImVec2(100, 30), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize({ui_scale*350, ui_scale*235}, ImGuiSetCond_FirstUseEver);  // We don't want to set it *every* time, the user might have preferences

        // Export window
        if ( ImGui::Begin(loc(TXT_export_DOTS), &opened, ImGuiWindowFlags_NoCollapse) ) {
            ImGui::Text(loc(TXT_MSG_click_and_drag_instruction));

            Exporter* exporter = &milton->gui->exporter;
            if ( exporter->state == ExporterState_SELECTED ) {
                i32 x = min( exporter->needle.x, exporter->pivot.x );
                i32 y = min( exporter->needle.y, exporter->pivot.y );
                int raster_w = MLT_ABS(exporter->needle.x - exporter->pivot.x);
                int raster_h = MLT_ABS(exporter->needle.y - exporter->pivot.y);

                ImGui::Text("%s: %dx%d\n",
                            loc(TXT_current_selection),
                            raster_w, raster_h);
                if (ImGui::InputInt(loc(TXT_scale_up), &exporter->scale, 1, /*step_fast=*/2)) {}
                if ( exporter->scale <= 0 ) {
                    exporter->scale = 1;
                }
                float viewport_limits[2] = {};
                gpu_get_viewport_limits(milton->renderer, viewport_limits);

                while ( exporter->scale*raster_w > viewport_limits[0]
                        || exporter->scale*raster_h > viewport_limits[1] ) {
                    --exporter->scale;
                }
                i32 max_scale = milton->view->scale / 2;
                if ( exporter->scale > max_scale) {
                    exporter->scale = max_scale;
                }
                ImGui::Text("%s: %dx%d\n", loc(TXT_final_image_resolution), raster_w*exporter->scale, raster_h*exporter->scale);

                ImGui::Text(loc(TXT_background_COLON));
                static int radio_v = 0;
                ImGui::RadioButton(loc(TXT_background_color), &radio_v, 0);
                ImGui::RadioButton(loc(TXT_transparent_background), &radio_v, 1);
                bool transparent_background = radio_v == 1;

                if ( ImGui::Button(loc(TXT_export_selection_to_image_DOTS)) ) {
                    // Render to buffer
                    int bpp = 4;  // bytes per pixel
                    i32 w = raster_w * exporter->scale;
                    i32 h = raster_h * exporter->scale;
                    size_t size = (size_t)w * h * bpp;
                    u8* buffer = (u8*)mlt_calloc(1, size, "Bitmap");
                    if ( buffer ) {
                        opened = false;
                        gpu_render_to_buffer(milton, buffer, exporter->scale,
                                             x,y, raster_w, raster_h, transparent_background ? 0.0f : 1.0f);
                        //milton_render_to_buffer(milton, buffer, x,y, raster_w, raster_h, exporter->scale);
                        PATH_CHAR* fname = platform_save_dialog(FileKind_IMAGE);
                        if ( fname ) {
                            milton_save_buffer_to_file(fname, buffer, w, h);
                        }
                        mlt_free (buffer, "Bitmap");
                    } else {
                        platform_dialog(loc(TXT_MSG_memerr_did_not_write), loc(TXT_error));
                    }
                }
            }
        }

        if ( ImGui::Button(loc(TXT_cancel)) ) {
            reset = true;
            milton_leave_mode(milton);
        }
        ImGui::End(); // Export...
        if ( !opened ) {
            reset = true;
            milton_leave_mode(milton);
        }
        if ( reset ) {
            exporter_init(&milton->gui->exporter);
        }
    } // exporting

#if MILTON_ENABLE_PROFILING
    ImGui::SetNextWindowPos(ImVec2(ui_scale*300, ui_scale*205), ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize({ui_scale*350, ui_scale*285}, ImGuiSetCond_FirstUseEver);  // We don't want to set it *every* time, the user might have preferences
    if ( milton->viz_window_visible ) {
        bool opened = true;
        if ( ImGui::Begin("Debug Data ([BACKQUOTE] to toggle)", &opened, ImGuiWindowFlags_NoCollapse) ) {
            float graph_height = 20;
            char msg[512] = {};

            float poll     = perf_count_to_sec(milton->graph_frame.polling) * 1000.0f;
            float update   = perf_count_to_sec(milton->graph_frame.update) * 1000.0f;
            float clipping = perf_count_to_sec(milton->graph_frame.clipping) * 1000.0f;
            float raster   = perf_count_to_sec(milton->graph_frame.raster) * 1000.0f;
            float GL       = perf_count_to_sec(milton->graph_frame.GL) * 1000.0f;
            float system   = perf_count_to_sec(milton->graph_frame.system) * 1000.0f;

            float sum = poll + update + raster + GL + system;

            snprintf(msg, array_count(msg),
                     "Input Polling %f ms\n",
                     poll);
            ImGui::Text(msg);

            snprintf(msg, array_count(msg),
                     "Milton Update %f ms\n",
                     update);
            ImGui::Text(msg);

            snprintf(msg, array_count(msg),
                     "Clipping & Update %f ms\n",
                     clipping);
            ImGui::Text(msg);

            snprintf(msg, array_count(msg),
                     "OpenGL commands %f ms\n",
                     GL);
            ImGui::Text(msg);

            snprintf(msg, array_count(msg),
                     "System %f ms \n",
                     system);
            ImGui::Text(msg);

            snprintf(msg, array_count(msg),
                     "Number of strokes in GPU memory: %d\n",
                     gpu_get_num_clipped_strokes(milton->canvas->root_layer));
            ImGui::Text(msg);

            float hist[] = { poll, update, raster, GL, system };
            ImGui::PlotHistogram("Graph",
                            (const float*)hist, array_count(hist));

            {
                static const int window_size = 100;
                static float moving_window[window_size] = {};
                static int window_i = 0;

                moving_window[window_i++] = sum;
                window_i %= window_size;

                float mavg = 0.0f;
                for ( int i =0; i < window_size; ++i ) {
                    mavg += moving_window[i];
                }
                mavg /= window_size;

                snprintf(msg, array_count(msg),
                         "Total %f ms (%f ms m. avg)\n",
                         sum,
                         mavg);

            }
            ImGui::Text(msg);

            ImGui::Dummy({0,30});

            i64 stroke_count = layer::count_strokes(milton->canvas->root_layer);

            auto* view = milton->view;
            int screen_height = view->screen_size.h * view->scale;
            int screen_width = view->screen_size.w * view->scale;

            if ( screen_height>0 && screen_height>0 ) {
                v2l pan = view->pan_center;

                i64 radius = ((i64)((1ull)<<63ull)-1);

                {
                    if ( pan.y > 0 ) {
                        i64 n_screens_below = ((i64)(radius) - (i64)pan.y)/(i64)screen_height;
                        snprintf(msg, array_count(msg),
                                 "Screens below: %I64d\n", n_screens_below);
                        ImGui::Text(msg);
                    } else {
                        i64 n_screens_above = ((i64)(radius) + (i64)pan.y)/(i64)screen_height;
                        snprintf(msg, array_count(msg),
                                 "Screens above: %I64d\n", n_screens_above);
                        ImGui::Text(msg);
                    }
                }
                {
                    if ( pan.x > 0 ) {
                        i64 n_screens_right = ((i64)(radius) - (i64)pan.x)/(i64)screen_width;
                        snprintf(msg, array_count(msg),
                                 "Screens to the right: %I64d\n", n_screens_right);
                        ImGui::Text(msg);
                    } else {
                        i64 n_screens_left = ((i64)(radius) + (i64)pan.x)/(i64)screen_width;
                        snprintf(msg, array_count(msg),
                                 "Screens to the left: %I64d\n", n_screens_left);
                        ImGui::Text(msg);
                    }
                }
            }
            snprintf(msg, array_count(msg),
                     "Scale: %lld", view->scale);
            ImGui::Text(msg);

            // Average stroke size.
            i64 avg = 0;
            {
                for ( Layer* l = milton->canvas->root_layer;
                      l != NULL;
                      l = l->next ) {
                    for ( i64 i = 0; i < l->strokes.count; ++i ) {
                        Stroke* s = get(&l->strokes, i);
                        avg += s->num_points;
                    }
                }
                if ( stroke_count > 0 ) {
                    avg /= stroke_count;
                }
            }
            snprintf(msg, array_count(msg),
                     "Average stroke size: %" PRIi64, avg);
            ImGui::Text(msg);

        } ImGui::End();
    } // profiling
#endif
    ImGui::PopStyleColor(color_stack);
}

static Rect
color_button_as_rect(const ColorButton* button)
{
    Rect rect = {};
    rect.left = button->x;
    rect.right = button->x + button->w;
    rect.top = button->y;
    rect.bottom = button->y + button->h;
    return rect;
}

static void
picker_update_points(ColorPicker* picker, float angle)
{
    picker->data.hsv.h = radians_to_degrees(angle);
    // Update the triangle
    float radius = 0.9f * (picker->wheel_radius - picker->wheel_half_width);
    v2f center = v2l_to_v2f(VEC2L(picker->center));
    {
        v2f point = polar_to_cartesian(-angle, radius);
        point = point + center;
        picker->data.c = point;
    }
    {
        v2f point = polar_to_cartesian(-angle + 2 * kPi / 3.0f, radius);
        point = point + center;
        picker->data.b = point;
    }
    {
        v2f point = polar_to_cartesian(-angle + 4 * kPi / 3.0f, radius);
        point = point + center;
        picker->data.a = point;
    }
}


static void
picker_update_wheel(ColorPicker* picker, v2f polar_point)
{
    float angle = picker_wheel_get_angle(picker, polar_point);
    picker_update_points(picker, angle);
}

static b32
picker_hits_triangle(ColorPicker* picker, v2f fpoint)
{
    b32 result = is_inside_triangle(fpoint, picker->data.a, picker->data.b, picker->data.c);
    return result;
}

static void
picker_deactivate(ColorPicker* picker)
{
    picker->flags = ColorPickerFlags_NOTHING;
}

static b32
is_inside_picker_rect(ColorPicker* picker, v2i point)
{
    return is_inside_rect(picker->bounds, point);
}

static Rect
picker_color_buttons_bounds(const ColorPicker* picker)
{
    Rect bounds = {};
    bounds.right = INT_MIN;
    bounds.left = INT_MAX;
    bounds.top = INT_MAX;
    bounds.bottom = INT_MIN;
    const ColorButton* button = picker->color_buttons;
    while ( button ) {
        bounds = rect_union(bounds, color_button_as_rect(button));
        button = button->next;
    }
    return bounds;
}

static b32
is_inside_picker_button_area(ColorPicker* picker, v2i point)
{
    Rect button_rect = picker_color_buttons_bounds(picker);
    b32 is_inside = is_inside_rect(button_rect, point);
    return is_inside;
}

b32
gui_point_hovers(MiltonGui* gui, v2i point)
{
    b32 hovers = gui->visible &&
                    (is_inside_picker_rect(&gui->picker, point) ||
                     is_inside_picker_button_area(&gui->picker, point));
    return hovers;
}

void
gui_picker_from_rgb(ColorPicker* picker, v3f rgb)
{
    v3f hsv = rgb_to_hsv(rgb);
    picker->data.hsv = hsv;
    float angle = hsv.h * 2*kPi;
    picker_update_points(picker, angle);
}

static void
update_button_bounds(ColorPicker* picker, f32 ui_scale)
{
    i32 bounds_radius_px = ui_scale*BOUNDS_RADIUS_PX;

    i32 spacing = 4*ui_scale;
    i32 num_buttons = NUM_BUTTONS;

    i32 button_size = (2*bounds_radius_px - (num_buttons - 1) * spacing) / num_buttons;
    i32 current_x = ui_scale*40 - button_size / 2;

    for ( ColorButton* cur_button = picker->color_buttons;
          cur_button != NULL;
          cur_button = cur_button->next ) {
        cur_button->x = current_x;
        cur_button->y = picker->center.y + bounds_radius_px + spacing;
        cur_button->w = button_size;
        cur_button->h = button_size;

        current_x += spacing + button_size;
    }
}

static b32
picker_hit_history_buttons(ColorPicker* picker, f32 ui_scale, v2i point)
{
    b32 hits = false;
    ColorButton* first = picker->color_buttons;
    ColorButton* button = first;
    ColorButton* prev = NULL;
    while ( button ) {
        if ( button->rgba.a != 0 &&
             is_inside_rect(color_button_as_rect(button), point) ) {
            hits = true;

            gui_picker_from_rgb(picker, button->rgba.rgb);

            if ( prev ) {
                prev->next = button->next;
            }
            if ( button != picker->color_buttons ) {
                button->next = picker->color_buttons;
            }
            picker->color_buttons = button;

            update_button_bounds(picker, ui_scale);
            break;
        }
        prev = button;
        button = button->next;
    }
    return hits;
}

static ColorPickResult
picker_update(ColorPicker* picker, v2i point)
{
    ColorPickResult result = ColorPickResult_NOTHING;
    v2f fpoint = v2i_to_v2f(point);
    if ( picker->flags == ColorPickerFlags_NOTHING ) {
        if ( picker_hits_wheel(picker, fpoint) ) {
            picker->flags |= ColorPickerFlags_WHEEL_ACTIVE;
        }
        else if ( picker_hits_triangle(picker, fpoint) ) {
            picker->flags |= ColorPickerFlags_TRIANGLE_ACTIVE;
        }
    }
    if (( picker->flags & ColorPickerFlags_WHEEL_ACTIVE )) {
        if (!(picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE))
        {
            picker_update_wheel(picker, fpoint);
            result = ColorPickResult_CHANGE_COLOR;
        }
    }
    if (( picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE )) {
        PickerData& d = picker->data;  // Just shortening the identifier.

        // We don't want the chooser to "stick" if it goes outside the triangle
        // (i.e. picking black should be easy)
        f32 abp = orientation(d.a, d.b, fpoint);
        f32 bcp = orientation(d.b, d.c, fpoint);
        f32 cap = orientation(d.c, d.a, fpoint);
        int insideness = (abp < 0.0f) + (bcp < 0.0f) + (cap < 0.0f);

        // inside the triangle
        if ( insideness == 3 ) {
            float inv_area = 1.0f / orientation(d.a, d.b, d.c);
            d.hsv.v = 1.0f - (cap * inv_area);

            d.hsv.s = abp * inv_area / d.hsv.v;
            d.hsv.s = abp  / (orientation(d.a, d.b, d.c) - cap);
            d.hsv.s = 1 - (bcp * inv_area) / d.hsv.v;
        }
        // near a corner
        else if ( insideness < 2 ) {
            if (abp < 0.0f) {
                d.hsv.s = 1.0f;
                d.hsv.v = 1.0f;
            }
            else if (bcp < 0.0f) {
                d.hsv.s = 0.0f;
                d.hsv.v = 1.0f;
            }
            else {
                d.hsv.s = 0.5f;
                d.hsv.v = 0.0f;
            }
        }
        // near an edge
        else {
#define GET_T(A, B, C)                            \
    v2f perp = perpendicular(fpoint - C); \
    f32 t = DOT(C - A, perp) / DOT(B - A, perp);
            if (abp >= 0.0f) {
                GET_T(d.b, d.a, d.c)
                d.hsv.s = 0.0f;
                d.hsv.v = t;
            }
            else if (bcp >= 0.0f) {
                GET_T(d.b, d.c, d.a)
                d.hsv.s = 1.0f;
                d.hsv.v = t;
            }
            else {
                GET_T(d.a, d.c, d.b)
                d.hsv.s = t;
                d.hsv.v = 1.0f;
            }
        }
#undef GET_T
        result = ColorPickResult_CHANGE_COLOR;
    }

    return result;
}


b32
picker_hits_wheel(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = point - center;
    float dist = magnitude(arrow);

    b32 hits = (dist <= picker->wheel_radius + picker->wheel_half_width ) &&
               (dist >= picker->wheel_radius - picker->wheel_half_width );

    return hits;
}

float
picker_wheel_get_angle(ColorPicker* picker, v2f point)
{
    v2f direction = point - v2i_to_v2f(picker->center);
    f32 angle = atan2f(direction.y, -direction.x) + kPi;
    return angle;
}

void
picker_init(ColorPicker* picker)
{
    v2f fpoint = {
        (f32)picker->center.x + (int)(picker->wheel_radius),
        (f32)picker->center.y
    };
    picker_update_wheel(picker, fpoint);
    picker->data.hsv = v3f{ 0, 1, 0 };
}

Rect
picker_get_bounds(ColorPicker* picker)
{
    Rect picker_rect;
    {
        picker_rect.left   = picker->center.x - picker->bounds_radius_px;
        picker_rect.right  = picker->center.x + picker->bounds_radius_px;
        picker_rect.bottom = picker->center.y + picker->bounds_radius_px;
        picker_rect.top    = picker->center.y - picker->bounds_radius_px;
    }
    mlt_assert (picker_rect.left >= 0);
    mlt_assert (picker_rect.top >= 0);

    return picker_rect;
}

void
exporter_init(Exporter* exporter)
{
    *exporter = Exporter{};
    exporter->scale = 1;
}

b32
exporter_input(Exporter* exporter, MiltonInput const* input)
{
    b32 changed = false;
    if ( input->input_count > 0 ) {
        changed = true;
        v2i point = VEC2I(input->points[input->input_count - 1]);
        if ( exporter->state == ExporterState_EMPTY ||
             exporter->state == ExporterState_SELECTED ) {
            exporter->pivot = point;
            exporter->needle = point;
            exporter->state = ExporterState_GROWING_RECT;
        }
        else if ( exporter->state == ExporterState_GROWING_RECT ) {
            exporter->needle = point;
        }
    }
    if ( (input->flags & MiltonInputFlags_END_STROKE) && exporter->state != ExporterState_EMPTY ) {
        exporter->state = ExporterState_SELECTED;
        changed = true;
    }
    return changed;
}

void
gui_imgui_set_ungrabbed(MiltonGui* gui)
{
    gui->flags &= ~MiltonGuiFlags_SHOWING_PREVIEW;
}

Rect
get_bounds_for_picker_and_colors(ColorPicker* picker)
{
    Rect result = rect_union(picker_get_bounds(picker), picker_color_buttons_bounds(picker));
    return result;
}

static b32
picker_is_active(ColorPicker* picker)
{
    b32 active = (picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE) ||
            (picker->flags & ColorPickerFlags_WHEEL_ACTIVE);
    return active;
}

v3f
picker_hsv_from_point(ColorPicker* picker, v2f point)
{
    float area = orientation(picker->data.a, picker->data.b, picker->data.c);
    v3f hsv = {};
    if ( area != 0 ) {
        float inv_area = 1.0f / area;
        float v = 1 - (orientation(point, picker->data.c, picker->data.a) * inv_area);
        float s = orientation(picker->data.b, point, picker->data.a) * inv_area / v;

        hsv = v3f
        {
            picker->data.hsv.h,
            s,
            v,
        };
    }
    return hsv;
}

v3f
gui_get_picker_rgb(MiltonGui* gui)
{
    v3f rgb = hsv_to_rgb(gui->picker.data.hsv);
    return rgb;
}

// Returns true if the Picker consumed input. False if the GUI wasn't affected
b32
gui_consume_input(MiltonGui* gui, MiltonInput const* input)
{
    b32 accepts = false;
    v2i point = VEC2I(input->points[0]);
    if ( gui->visible ) {
        accepts = gui_point_hovers(gui, point);
        if ( !picker_is_active(&gui->picker) &&
             !gui->did_hit_button &&
             picker_hit_history_buttons(&gui->picker, gui->scale, point) ) {
            accepts = true;
            gui->did_hit_button = true;
            gui->owns_user_input = true;
        }
        if ( accepts ) {
            ColorPickResult pick_result = picker_update(&gui->picker, point);
            if ( pick_result == ColorPickResult_CHANGE_COLOR ) {
                gui->owns_user_input = true;
            }
        }
    }
    return accepts;
}

void
gui_toggle_menu_visibility(MiltonGui* gui)
{
    gui->menu_visible = !gui->menu_visible;
}

void
gui_toggle_help(MiltonGui* gui)
{
    gui->show_help_widget = !gui->show_help_widget;
}

void
gui_init(Arena* root_arena, MiltonGui* gui, f32 ui_scale)
{
    gui->scale = ui_scale;
    i32 bounds_radius_px = ui_scale*BOUNDS_RADIUS_PX;
    f32 wheel_half_width = ui_scale*12;
    gui->picker.center = v2i{ bounds_radius_px + int(ui_scale*20), bounds_radius_px + (int)(ui_scale*30) };
    gui->picker.bounds_radius_px = bounds_radius_px;
    gui->picker.wheel_half_width = wheel_half_width;
    gui->picker.wheel_radius = (f32)bounds_radius_px - ui_scale*5.0f - wheel_half_width;
    gui->picker.data.hsv = v3f{ 0.0f, 1.0f, 0.7f };
    Rect bounds;
    bounds.left = gui->picker.center.x - bounds_radius_px;
    bounds.right = gui->picker.center.x + bounds_radius_px;
    bounds.top = gui->picker.center.y - bounds_radius_px;
    bounds.bottom = gui->picker.center.y + bounds_radius_px;
    gui->picker.bounds = bounds;
    gui->picker.pixels = arena_alloc_array(root_arena, (4 * bounds_radius_px * bounds_radius_px), u32);
    gui->visible = true;
    gui->picker.color_buttons = arena_alloc_elem(root_arena, ColorButton);
    gui->original_settings = arena_alloc_elem(root_arena, MiltonSettings);

    picker_init(&gui->picker);

    i32 num_buttons = NUM_BUTTONS;
    auto* cur_button = gui->picker.color_buttons;
    for ( i64 i = 0; i != (num_buttons - 1); ++i ) {
        cur_button->next = arena_alloc_elem(root_arena, ColorButton);
        cur_button = cur_button->next;
    }
    update_button_bounds(&gui->picker, gui->scale);

    gui->preview_pos      = v2i{-1, -1};
    gui->preview_pos_prev = v2i{-1, -1};


    exporter_init(&gui->exporter);
}

// When a selected color is used in a stroke, call this to update the color
// button list.
b32
gui_mark_color_used(MiltonGui* gui)
{
    b32 changed = false;
    ColorButton* start = gui->picker.color_buttons;
    v3f picker_color  = hsv_to_rgb(gui->picker.data.hsv);
    // Search for a color that is already in the list
    ColorButton* button = start;
    while ( button ) {
        if ( button->rgba.a != 0 ) {
            v3f diff =
            {
                fabsf(button->rgba.r - picker_color.r),
                fabsf(button->rgba.g - picker_color.g),
                fabsf(button->rgba.b - picker_color.b),
            };
            float epsilon = 0.000001f;
            if ( diff.r < epsilon && diff.g < epsilon && diff.b < epsilon ) {
                // Move this button to the start and return.
                changed = true;
                v4f tmp_color = button->rgba;
                button->rgba = start->rgba;
                start->rgba = tmp_color;
            }
        }
        button = button->next;
    }
    button = start;

    // If not found, add to list.
    if ( !changed ) {
        changed = true;
        v4f button_color = color_rgb_to_rgba(picker_color,1);
        // Pass data to the next one.
        while ( button ) {
            v4f tmp_color = button->rgba;
            button->rgba = button_color;
            button_color = tmp_color;
            button = button->next;
        }
    }

    return changed;
}

void
gui_deactivate(MiltonGui* gui)
{
    picker_deactivate(&gui->picker);

    // Reset transient values
    gui->owns_user_input = false;
    gui->did_hit_button = false;
}
