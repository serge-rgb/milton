// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(push, 0)
#endif  // _WIN32 && _MSC_VER

#include <imgui.h>

#include "gui.h"
#include "localization.h"
#include "platform.h"
#include "rasterizer.h"
#include "persist.h"

static void exporter_init(Exporter* exporter)
{
    *exporter = {};
    exporter->scale = 1;
}

void milton_gui_tick(MiltonInput* input, MiltonState* milton_state)
{
    // ImGui Section
    auto default_imgui_window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

    const float pen_alpha = milton_get_pen_alpha(milton_state);
    assert(pen_alpha >= 0.0f && pen_alpha <= 1.0f);
    // Spawn below the picker
    Rect pbounds = get_bounds_for_picker_and_colors(&milton_state->gui->picker);

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
        if ( ImGui::BeginMenu(LOC(file)) ) {
            if ( ImGui::MenuItem(LOC(open_milton_canvas)) ) {

            }
            if ( ImGui::MenuItem(LOC(export_to_image_DOTS)) ) {
                milton_switch_mode(milton_state, MiltonMode_EXPORTING);
            }
            if ( ImGui::MenuItem(LOC(quit)) ) {
                milton_try_quit(milton_state);
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu(LOC(canvas), /*enabled=*/true) ) {
            //if ( ImGui::MenuItem(LOC(set_background_color)) ) {
            if ( ImGui::BeginMenu(LOC(set_background_color)) ) {
                //ImGui::SetWindowSize({271, 109}, ImGuiSetCond_Always);
                v3f bg = milton_state->view->background_color;
                if ( ImGui::ColorEdit3(LOC(color), bg.d) ) {
                    milton_state->view->background_color = clamp_01(bg);
                    i32 f = input->flags;
                    set_flag(f, (i32)MiltonInputFlags_FULL_REFRESH);
                    set_flag(f, (i32)MiltonInputFlags_FAST_DRAW);
                    input->flags = (MiltonInputFlags)f;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu(LOC(view)) ) {
            if ( ImGui::MenuItem(LOC(toggle_gui_visibility)) ) {
                gui_toggle_visibility(milton_state->gui);
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu(LOC(help)) ) {
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

        const f32 brush_windwow_height = 109;
        ImGui::SetNextWindowPos(ImVec2(10, 10 + (float)pbounds.bottom), ImGuiSetCond_Always);
        ImGui::SetNextWindowSize({271, brush_windwow_height}, ImGuiSetCond_Always);  // We don't want to set it *every* time, the user might have preferences

        b32 show_brush_window = (milton_state->current_mode == MiltonMode_PEN || milton_state->current_mode == MiltonMode_ERASER);

        // Brush Window
        if ( show_brush_window) {
            if ( ImGui::Begin(LOC(brushes), NULL, default_imgui_window_flags) ) {
                if ( milton_state->current_mode == MiltonMode_PEN ) {
                    float mut_alpha = pen_alpha;
                    ImGui::SliderFloat(LOC(opacity), &mut_alpha, 0.1f, 1.0f);
                    if ( mut_alpha != pen_alpha ) {
                        milton_set_pen_alpha(milton_state, mut_alpha);
                        i32 f = milton_state->gui->flags;
                        set_flag(f, (i32)MiltonGuiFlags_SHOWING_PREVIEW);
                        milton_state->gui->flags = (MiltonGuiFlags)f;
                    }
                }

                const auto size = milton_get_brush_size(milton_state);
                auto mut_size = size;

                ImGui::SliderInt(LOC(brush_size), &mut_size, 1, k_max_brush_size);

                if ( mut_size != size ) {
                    milton_set_brush_size(milton_state, mut_size);
                    i32 f = milton_state->gui->flags;
                    set_flag(f, (i32)MiltonGuiFlags_SHOWING_PREVIEW);
                    milton_state->gui->flags = (MiltonGuiFlags)f;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1,1,1,1});
                {
                    if ( milton_state->current_mode != MiltonMode_PEN ) {
                        if ( ImGui::Button(LOC(switch_to_pen)) ) {
                            i32 f = input->flags;
                            set_flag(f, MiltonInputFlags_CHANGE_MODE);
                            input->flags = (MiltonInputFlags)f;
                            input->mode_to_set = MiltonMode_PEN;
                        }
                    }

                    if ( milton_state->current_mode != MiltonMode_ERASER ) {
                        if ( ImGui::Button(LOC(switch_to_eraser)) ) {
                            i32 f = input->flags;
                            set_flag(f, (i32)MiltonInputFlags_CHANGE_MODE);
                            input->flags = (MiltonInputFlags)f;
                            input->mode_to_set = MiltonMode_ERASER;
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
            if ( check_flag(milton_state->gui->flags, MiltonGuiFlags_SHOWING_PREVIEW) ) {
                milton_state->gui->preview_pos = pos;
            }
        }


        // Layer window
        ImGui::SetNextWindowPos(ImVec2(10, 20 + (float)pbounds.bottom + brush_windwow_height ), ImGuiSetCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiSetCond_FirstUseEver);
        if ( ImGui::Begin(LOC(layers)) ) {
            CanvasView* view = milton_state->view;
            // left
            ImGui::BeginChild("left pane", ImVec2(150, 0), true);

            Layer* layer = milton_state->root_layer;
            while ( layer->next ) { layer = layer->next; }  // Move to the top layer.
            while ( layer ) {
                bool v = layer->flags & LayerFlags_VISIBLE;
                ImGui::PushID(layer->id);
                if ( ImGui::Checkbox("##select", &v) ) {
                    layer_toggle_visibility(layer);
                    input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;
                    input->flags |= (i32)MiltonInputFlags_FAST_DRAW;
                }
                ImGui::PopID();
                ImGui::SameLine();
                if ( ImGui::Selectable(layer->name, milton_state->working_layer == layer) ) {
                    milton_set_working_layer(milton_state, layer);
                }
                layer = layer->prev;

            }
            ImGui::EndChild();
            ImGui::SameLine();

            // right
            ImGui::BeginGroup();
            //ImGui::BeginChild("item view", ImVec2(0, -30-ImGui::GetItemsLineHeightWithSpacing()));
            ImGui::BeginChild("item view", ImVec2(0, 25));
            if ( ImGui::Button("New Layer") ) {
                milton_new_layer(milton_state);
            }
            ImGui::Separator();
            ImGui::EndChild();
            ImGui::BeginChild("buttons");

            static b32 is_renaming = false;
            if ( is_renaming == false ) {
                ImGui::Text(milton_state->working_layer->name);
                ImGui::Indent();
                if ( ImGui::Button("Rename") ) {
                    is_renaming = true;
                }
                ImGui::Unindent();
            }
            else if ( is_renaming ) {
                if ( ImGui::InputText("##rename",
                                      milton_state->working_layer->name,
                                      13,
                                      //MAX_LAYER_NAME_LEN,
                                      ImGuiInputTextFlags_EnterReturnsTrue
                                      //,ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL
                                     ) ) {
                    is_renaming = false;
                }
                ImGui::SameLine();
                if ( ImGui::Button("Ok") ) {
                    is_renaming = false;
                }
            }
            ImGui::Text("Move");

            Layer* a = NULL;
            Layer* b = NULL;
            if ( ImGui::Button("Up") ) {
                b = milton_state->working_layer;
                a = b->next;
            }
            ImGui::SameLine();
            if ( ImGui::Button("Down") ) {
                a = milton_state->working_layer;
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
                while ( milton_state->root_layer->prev ) {
                    milton_state->root_layer = milton_state->root_layer->prev;
                }
                input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;
                input->flags |= (i32)MiltonInputFlags_FAST_DRAW;
            }

            static bool deleting = false;
            if ( deleting == false ) {
                if ( ImGui::Button("Delete") ) {
                    deleting = true;
                }
            }
            else if ( deleting ) {
                ImGui::Text("Are you sure?");
                ImGui::Text("Can't be undone.");
                if ( ImGui::Button("Yes") ) {
                    milton_delete_working_layer(milton_state);
                    deleting = false;
                }
                ImGui::SameLine();
                if ( ImGui::Button("No") ) {
                    deleting = false;
                }
            }
            ImGui::EndChild();
            ImGui::EndGroup();

        } ImGui::End();
    } // Visible

    // Note: The export window is drawn regardless of gui visibility.
    if ( milton_state->current_mode == MiltonMode_EXPORTING ) {
        bool opened = true;
        b32 reset = false;

        ImGui::SetNextWindowPos(ImVec2(100, 30), ImGuiSetCond_Once);
        ImGui::SetNextWindowSize({350, 235}, ImGuiSetCond_Always);  // We don't want to set it *every* time, the user might have preferences

        // Export window
        if ( ImGui::Begin(LOC(export_DOTS), &opened, ImGuiWindowFlags_NoCollapse) ) {
            ImGui::Text(LOC(MSG_click_and_drag_instruction));

            Exporter* exporter = &milton_state->gui->exporter;
            if ( exporter->state == ExporterState_SELECTED ) {
                i32 x = min( exporter->needle.x, exporter->pivot.x );
                i32 y = min( exporter->needle.y, exporter->pivot.y );
                int raster_w = abs(exporter->needle.x - exporter->pivot.x);
                int raster_h = abs(exporter->needle.y - exporter->pivot.y);

                ImGui::Text("%s: %dx%d\n",
                            LOC(current_selection),
                            raster_w, raster_h);
                if ( ImGui::InputInt(LOC(scale_up), &exporter->scale, 1, /*step_fast=*/2) ) {}
                if ( exporter->scale <= 0 ) exporter->scale = 1;
                i32 max_scale = milton_state->view->scale / 2;
                if ( exporter->scale > max_scale) {
                    exporter->scale = max_scale;
                }
                ImGui::Text("%s: %dx%d\n", LOC(final_image_size), raster_w*exporter->scale, raster_h*exporter->scale);

                if ( ImGui::Button(LOC(export_selection_to_image_DOTS)) ) {
                    // Render to buffer
                    int bpp = 4;  // bytes per pixel
                    i32 w = raster_w * exporter->scale;
                    i32 h = raster_h * exporter->scale;
                    size_t size = (size_t)w * h * bpp;
                    u8* buffer = (u8*)mlt_malloc(size);
                    if ( buffer ) {
                        opened = false;
                        milton_render_to_buffer(milton_state, buffer, x,y, raster_w, raster_h, exporter->scale);
                        char* fname = platform_save_dialog();
                        if ( fname ) {
                            milton_save_buffer_to_file(fname, buffer, w, h);
                        }
                        mlt_free (buffer);
                    } else {
                        platform_dialog(LOC(MSG_memerr_did_not_write),
                                        LOC(error));
                    }
                }
            }
        }

        if ( ImGui::Button(LOC(cancel)) ) {
            reset = true;
            milton_use_previous_mode(milton_state);
        }
        ImGui::End(); // Export...
        if ( !opened ) {
            reset = true;
            milton_use_previous_mode(milton_state);
        }
        if ( reset ) {
            exporter_init(&milton_state->gui->exporter);
        }
    }

    // Shortcut help. Also shown regardless of UI visibility.
    // TODO: Remove this.
    if ( milton_state->gui->show_help_widget ) {
        //bool opened;
        ImGui::SetNextWindowPos(ImVec2(365, 92), ImGuiSetCond_Always);
        ImGui::SetNextWindowSize({235, 235}, ImGuiSetCond_Always);  // We don't want to set it *every* time, the user might have preferences
        bool opened = true;
        if ( ImGui::Begin("Shortcuts", &opened, default_imgui_window_flags) ) {
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
            if ( !opened ) {
                milton_state->gui->show_help_widget = false;
            }
        }
        ImGui::End();  // Help
    }


    ImGui::PopStyleColor(color_stack);

}


#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(pop)
#endif  // _WIN32 && _MSC_VER
