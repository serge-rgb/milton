// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


static void milton_imgui_tick(MiltonInput* input, PlatformState* platform_state,  MiltonState* milton_state)
{
    // ImGui Section
    auto default_imgui_window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

    const float pen_alpha = milton_get_pen_alpha(milton_state);
    mlt_assert(pen_alpha >= 0.0f && pen_alpha <= 1.0f);
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

    // Menu ----
    int menu_style_stack = 0;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,        ImVec4{.3f,.3f,.3f,1}); ++menu_style_stack;
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,   ImVec4{.9f,.3f,.3f,1}); ++menu_style_stack;
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   ImVec4{.3f,.3f,.6f,1}); ++menu_style_stack;
    // TODO: translate
    char* default_will_be_lost = "The default canvas will be cleared. Save it?";
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu(LOC(file)))
        {
            if (ImGui::MenuItem(LOC(new_milton_canvas)))
            {
                b32 save_file = false;
                if (count_strokes(milton_state->root_layer) > 0)
                {
                    if (milton_state->flags & MiltonStateFlags_DEFAULT_CANVAS)
                    {
                        save_file = platform_dialog_yesno(default_will_be_lost, "Save?");
                    }
                }
                if (save_file)
                {
                    PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                    if (name)
                    {
                        milton_log("Saving to %s\n", name);
                        milton_set_canvas_file(milton_state, name);
                        milton_save(milton_state);
                        b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"), DeleteErrorTolerance_OK_NOT_EXIST);
                        if (del == false)
                        {
                            platform_dialog("Could not delete contents. The work will be still be there even though you saved it to a file.",
                                            "Info");
                        }
                    }
                }

                // New Canvas
                milton_reset_canvas_and_set_default(milton_state);
                input->flags |= MiltonInputFlags_FULL_REFRESH;
                milton_state->flags |= MiltonStateFlags_DEFAULT_CANVAS;
            }
            b32 save_requested = false;
            if ( ImGui::MenuItem(LOC(open_milton_canvas)) )
            {
                // If current canvas is MiltonPersist, then prompt to save
                if ( (milton_state->flags & MiltonStateFlags_DEFAULT_CANVAS) )
                {
                    b32 save_file = false;
                    if (count_strokes(milton_state->root_layer) > 0)
                    {
                        save_file = platform_dialog_yesno(default_will_be_lost, "Save?");
                    }
                    if (save_file)
                    {
                        PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                        if (name)
                        {
                            milton_log("Saving to %s\n", name);
                            milton_set_canvas_file(milton_state, name);
                            milton_save(milton_state);
                            b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"),
                                                                     DeleteErrorTolerance_OK_NOT_EXIST);
                            if (del == false)
                            {
                                platform_dialog("Could not delete default canvas. Contents will be still there when you create a new canvas.",
                                                "Info");
                            }
                        }
                    }
                }
                PATH_CHAR* fname = platform_open_dialog(FileKind_MILTON_CANVAS);
                if (fname)
                {
                    milton_set_canvas_file(milton_state, fname);
                    input->flags |= MiltonInputFlags_OPEN_FILE;
                    // TODO: Check if this line can be removed after switching to HW rendering.
                    milton_state->gui->flags |= MiltonGuiFlags_NEEDS_REDRAW;
                }
            }
            if (ImGui::MenuItem(LOC(save_milton_canvas_as_DOTS)) || save_requested)
            {
                // NOTE(possible refactor): There is a copy of this at milton.c end of file
                PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                if (name)
                {
                    milton_log("Saving to %s\n", name);
                    milton_set_canvas_file(milton_state, name);
                    input->flags |= MiltonInputFlags_SAVE_FILE;
                    b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"),
                                                             DeleteErrorTolerance_OK_NOT_EXIST);
                    if (del == false)
                    {
                        platform_dialog("Could not delete default canvas. Contents will be still there when you create a new canvas.",
                                        "Info");
                    }
                }
            }
            if (ImGui::MenuItem(LOC(export_to_image_DOTS)))
            {
                milton_switch_mode(milton_state, MiltonMode_EXPORTING);
            }
            if (ImGui::MenuItem(LOC(quit)))
            {
                milton_try_quit(milton_state);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(LOC(edit)))
        {
            if (ImGui::MenuItem(LOC(undo) ))
            {
                input->flags |= MiltonInputFlags_UNDO;
            }
            if (ImGui::MenuItem(LOC(redo)))
            {
                input->flags |= MiltonInputFlags_REDO;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(LOC(canvas), /*enabled=*/true))
        {
            if (ImGui::BeginMenu(LOC(set_background_color)))
            {
                v3f bg = milton_state->view->background_color;
                if (ImGui::ColorEdit3(LOC(color), bg.d))
                {
                    milton_set_background_color(milton_state, clamp_01(bg));
                    input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;
                    input->flags |= (i32)MiltonInputFlags_FAST_DRAW;
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(LOC(zoom_in)))
            {
                input->scale++;
            }
            if (ImGui::MenuItem(LOC(zoom_out)))
            {
                input->scale--;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(LOC(tools)))
        {
            // Brush
            if (ImGui::MenuItem(LOC(brush)))
            {
                input->flags |= MiltonInputFlags_CHANGE_MODE;
                input->mode_to_set = MiltonMode_PEN;
            }
            if (ImGui::BeginMenu(LOC(brush_options)))
            {
                b32 smoothing_enabled = milton_brush_smoothing_enabled(milton_state);
                char* entry_str = smoothing_enabled? LOC(disable_stroke_smoothing) : LOC(enable_stroke_smoothing);

                if (ImGui::MenuItem(entry_str))
                {
                    milton_toggle_brush_smoothing(milton_state);
                }
                // Decrease / increase brush size
                if (ImGui::MenuItem(LOC(decrease_brush_size)))
                {
                    for(int i=0;i<5;++i) milton_decrease_brush_size(milton_state);
                }
                if (ImGui::MenuItem(LOC(increase_brush_size)))
                {
                    for(int i=0;i<5;++i) milton_increase_brush_size(milton_state);
                }
                // Opacity shortcuts

                f32 opacities[] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
                for (i32 i = 0; i < array_count(opacities); ++i)
                {
                    char entry[128] = {0};
                    snprintf(entry, array_count(entry), "%s %d%% - [%d]",
                             LOC(set_opacity_to), (int)(100 * opacities[i]), i == 9 ? 0 : i+1);
                    if (ImGui::MenuItem(entry))
                    {
                        milton_set_pen_alpha(milton_state, opacities[i]);
                    }
                }
                ImGui::EndMenu();
            }
            // Eraser
            if ( ImGui::MenuItem(LOC(eraser)) )
            {
                input->flags |= MiltonInputFlags_CHANGE_MODE;
                input->mode_to_set = MiltonMode_ERASER;
            }
            // Panning
            char* move_str = platform_state->is_panning==false? LOC(move_canvas) : LOC(stop_moving_canvas);
            if (ImGui::MenuItem(move_str))
            {
                platform_state->is_panning = !platform_state->is_panning;
                platform_state->panning_locked = true;
            }
            // Eye Dropper
            if ( ImGui::MenuItem(LOC(eye_dropper)) )
            {
                input->flags |= MiltonInputFlags_CHANGE_MODE;
                input->mode_to_set = MiltonMode_EYEDROPPER;
                milton_state->flags |= MiltonStateFlags_IGNORE_NEXT_CLICKUP;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(LOC(view)))
        {
            if (ImGui::MenuItem(LOC(toggle_gui_visibility)))
            {
                gui_toggle_visibility(milton_state);
            }
#if MILTON_ENABLE_PROFILING
            if (ImGui::MenuItem("Toggle Debug Data [BACKQUOTE]"))
            {
                milton_state->viz_window_visible = !milton_state->viz_window_visible;
            }
#endif
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(LOC(help)))
        {
            if (ImGui::MenuItem(LOC(help_me)))
            {
                //platform_open_link("https://www.youtube.com/watch?v=g27gHio2Ohk");
                platform_open_link("http://www.miltonpaint.com/help/");
            }
            if (ImGui::MenuItem(LOC(milton_version)))
            {
                char buffer[1024];
                snprintf(buffer, array_count(buffer),
                         "Milton version %d.%d.%d",
                         MILTON_MAJOR_VERSION, MILTON_MINOR_VERSION, MILTON_MICRO_VERSION);
                platform_dialog(buffer, "Milton Version");
            }
            if ( ImGui::MenuItem(LOC(website)) )
            {
                platform_open_link("http://miltonpaint.com");
            }
            ImGui::EndMenu();
        }
        PATH_CHAR* utf16_name = str_trim_to_last_slash(milton_state->mlt_file_path);

        char file_name[MAX_PATH] = {};
        utf16_to_utf8_simple(utf16_name, file_name);

        char msg[1024];
        WallTime lst = milton_state->last_save_time;

        snprintf(msg, 1024, "\t%s -- Last saved: %.2d:%.2d:%.2d",
                 (milton_state->flags & MiltonStateFlags_DEFAULT_CANVAS) ? "[Default canvas]" :
                 file_name,
                 lst.hours, lst.minutes, lst.seconds);

        if (ImGui::BeginMenu(msg, /*bool enabled = */false))
        {
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleColor(menu_style_stack);


    // GUI Windows ----

    b32 should_show_windows = milton_state->current_mode != MiltonMode_EYEDROPPER &&
                                milton_state->current_mode != MiltonMode_EXPORTING;

    if (milton_state->gui->visible && should_show_windows)
    {

        /* ImGuiSetCond_Always        = 1 << 0, // Set the variable */
        /* ImGuiSetCond_Once          = 1 << 1, // Only set the variable on the first call per runtime session */
        /* ImGuiSetCond_FirstUseEver */

        const f32 brush_windwow_height = 109;
        ImGui::SetNextWindowPos(ImVec2(10, 10 + (float)pbounds.bottom), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize({271, brush_windwow_height}, ImGuiSetCond_FirstUseEver);  // We don't want to set it *every* time, the user might have preferences

        b32 show_brush_window = (milton_state->current_mode == MiltonMode_PEN || milton_state->current_mode == MiltonMode_ERASER);

        // Brush Window
        if (show_brush_window)
        {
            if (ImGui::Begin(LOC(brushes), NULL, default_imgui_window_flags))
            {
                if (milton_state->current_mode == MiltonMode_PEN)
                {
                    float mut_alpha = pen_alpha*100;
                    ImGui::SliderFloat(LOC(opacity), &mut_alpha, 1, 100, "%.0f%%");

                    mut_alpha /= 100.0f;
                    if (mut_alpha > 1.0f ) mut_alpha = 1.0f;
                    if (mut_alpha != pen_alpha)
                    {
                        milton_set_pen_alpha(milton_state, mut_alpha);
                        milton_state->gui->flags |= (i32)MiltonGuiFlags_SHOWING_PREVIEW;
                    }
                }

                const auto size = milton_get_brush_radius(milton_state);
                auto mut_size = size;

                ImGui::SliderInt(LOC(brush_size), &mut_size, 1, MILTON_MAX_BRUSH_SIZE);

                if (mut_size != size)
                {
                    milton_set_brush_size(milton_state, mut_size);
                    milton_state->gui->flags |= (i32)MiltonGuiFlags_SHOWING_PREVIEW;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1,1,1,1});
                {
                    if ( milton_state->current_mode != MiltonMode_PEN )
                    {
                        if ( ImGui::Button(LOC(switch_to_brush)) )
                        {
                            i32 f = input->flags;
                            f |= MiltonInputFlags_CHANGE_MODE;
                            input->flags = (MiltonInputFlags)f;
                            input->mode_to_set = MiltonMode_PEN;
                        }
                    }

                    if ( milton_state->current_mode != MiltonMode_ERASER )
                    {
                        if ( ImGui::Button(LOC(switch_to_eraser)) )
                        {
                            input->flags |= MiltonInputFlags_CHANGE_MODE;
                            input->mode_to_set = MiltonMode_ERASER;
                        }
                    }
                }
                ImGui::PopStyleColor(1); // Pop white button text

                // ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            }
            // Important to place this before ImGui::End()
            const v2i pos =
            {
                (i32)(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x + milton_get_brush_radius(milton_state)),
                (i32)(ImGui::GetWindowPos().y),
            };
            ImGui::End();  // Brushes
            if ((milton_state->gui->flags & MiltonGuiFlags_SHOWING_PREVIEW))
            {
                milton_state->gui->preview_pos = pos;
            }
        }


        // Layer window
        ImGui::SetNextWindowPos(ImVec2(10, 20 + (float)pbounds.bottom + brush_windwow_height ), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 220), ImGuiSetCond_FirstUseEver);
        if ( ImGui::Begin(LOC(layers)) )
        {
            CanvasView* view = milton_state->view;
            // left
            ImGui::BeginChild("left pane", ImVec2(150, 0), true);

            Layer* layer = milton_state->root_layer;
            while ( layer->next ) { layer = layer->next; }  // Move to the top layer.
            while ( layer )
            {
                bool v = layer->flags & LayerFlags_VISIBLE;
                ImGui::PushID(layer->id);
                if ( ImGui::Checkbox("##select", &v) )
                {
                    layer_toggle_visibility(layer);
                    input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;
                    input->flags |= (i32)MiltonInputFlags_FAST_DRAW;
                }
                ImGui::PopID();
                ImGui::SameLine();
                if ( ImGui::Selectable(layer->name, milton_state->working_layer == layer) )
                {
                    milton_set_working_layer(milton_state, layer);
                }
                layer = layer->prev;
            }
            ImGui::EndChild();
            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::BeginChild("item view", ImVec2(0, 25));
            if ( ImGui::Button(LOC(new_layer)) )
            {
                milton_new_layer(milton_state);
            }
            ImGui::Separator();
            ImGui::EndChild();
            ImGui::BeginChild("buttons");

            static b32 is_renaming = false;
            if (is_renaming == false)
            {
                ImGui::Text(milton_state->working_layer->name);
                ImGui::Indent();
                if ( ImGui::Button(LOC(rename)) )
                {
                    is_renaming = true;
                }
                ImGui::Unindent();
            }
            else if (is_renaming)
            {
                if (ImGui::InputText("##rename",
                                      milton_state->working_layer->name,
                                      13,
                                      //MAX_LAYER_NAME_LEN,
                                      ImGuiInputTextFlags_EnterReturnsTrue
                                      //,ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL
                                     ))
                {
                    is_renaming = false;
                }
                ImGui::SameLine();
                if ( ImGui::Button(LOC(ok)) )
                {
                    is_renaming = false;
                }
            }
            ImGui::Text(LOC(move));

            Layer* a = NULL;
            Layer* b = NULL;
            if ( ImGui::Button(LOC(up)) )
            {
                b = milton_state->working_layer;
                a = b->next;
            }
            ImGui::SameLine();
            if ( ImGui::Button(LOC(down)) )
            {
                a = milton_state->working_layer;
                b = a->prev;
            }
            if ( a && b )
            {
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
                while ( milton_state->root_layer->prev )
                {
                    milton_state->root_layer = milton_state->root_layer->prev;
                }
                input->flags |= (i32)MiltonInputFlags_FULL_REFRESH;
                input->flags |= (i32)MiltonInputFlags_FAST_DRAW;
            }

            if ( milton_state->working_layer->next ||
                 milton_state->working_layer->prev )
            {
                static bool deleting = false;
                if ( deleting == false )
                {
                    if ( ImGui::Button(LOC(delete)) )
                    {
                        deleting = true;
                    }
                }
                else if ( deleting )
                {
                    ImGui::Text(LOC(are_you_sure));
                    ImGui::Text(LOC(cant_be_undone));
                    if ( ImGui::Button(LOC(yes)) )
                    {
                        milton_delete_working_layer(milton_state);
                        deleting = false;
                    }
                    ImGui::SameLine();
                    if ( ImGui::Button(LOC(no)) )
                    {
                        deleting = false;
                    }
                }
            }
            ImGui::EndChild();
            ImGui::EndGroup();

        } ImGui::End();
    } // Visible

    // Note: The export window is drawn regardless of gui visibility.
    if ( milton_state->current_mode == MiltonMode_EXPORTING )
    {
        bool opened = true;
        b32 reset = false;

        ImGui::SetNextWindowPos(ImVec2(100, 30), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize({350, 235}, ImGuiSetCond_FirstUseEver);  // We don't want to set it *every* time, the user might have preferences

        // Export window
        if (ImGui::Begin(LOC(export_DOTS), &opened, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::Text(LOC(MSG_click_and_drag_instruction));

            Exporter* exporter = &milton_state->gui->exporter;
            if ( exporter->state == ExporterState_SELECTED )
            {
                i32 x = min( exporter->needle.x, exporter->pivot.x );
                i32 y = min( exporter->needle.y, exporter->pivot.y );
                int raster_w = abs(exporter->needle.x - exporter->pivot.x);
                int raster_h = abs(exporter->needle.y - exporter->pivot.y);

                ImGui::Text("%s: %dx%d\n",
                            LOC(current_selection),
                            raster_w, raster_h);
                if (ImGui::InputInt(LOC(scale_up), &exporter->scale, 1, /*step_fast=*/2)) {}
                if (exporter->scale <= 0)
                {
                    exporter->scale = 1;
                }
                while (exporter->scale*raster_w*SSAA_FACTOR > milton_state->render_data->viewport_limits[0] ||
                       exporter->scale*raster_h*SSAA_FACTOR > milton_state->render_data->viewport_limits[1])
                {
                    --exporter->scale;
                }
                i32 max_scale = milton_state->view->scale / 2;
                if ( exporter->scale > max_scale)
                {
                    exporter->scale = max_scale;
                }
                ImGui::Text("%s: %dx%d\n", LOC(final_image_size), raster_w*exporter->scale, raster_h*exporter->scale);

                if (ImGui::Button(LOC(export_selection_to_image_DOTS)))
                {
                    // Render to buffer
                    int bpp = 4;  // bytes per pixel
                    i32 w = raster_w * exporter->scale;
                    i32 h = raster_h * exporter->scale;
                    size_t size = (size_t)w * h * bpp;
                    u8* buffer = (u8*)mlt_malloc(size);
                    if ( buffer )
                    {
                        opened = false;
                        gpu_render_to_buffer(milton_state, buffer, exporter->scale,
                                             x*SSAA_FACTOR,y*SSAA_FACTOR, raster_w*SSAA_FACTOR, raster_h*SSAA_FACTOR);
                        //milton_render_to_buffer(milton_state, buffer, x,y, raster_w, raster_h, exporter->scale);
                        PATH_CHAR* fname = platform_save_dialog(FileKind_IMAGE);
                        if (fname)
                        {
                            milton_save_buffer_to_file(fname, buffer, w, h);
                        }
                        mlt_free (buffer);
                    }
                    else
                    {
                        platform_dialog(LOC(MSG_memerr_did_not_write), LOC(error));
                    }
                }
            }
        }

        if ( ImGui::Button(LOC(cancel)) )
        {
            reset = true;
            milton_use_previous_mode(milton_state);
        }
        ImGui::End(); // Export...
        if ( !opened )
        {
            reset = true;
            milton_use_previous_mode(milton_state);
        }
        if ( reset )
        {
            exporter_init(&milton_state->gui->exporter);
        }
    }
#if MILTON_ENABLE_PROFILING
    ImGui::SetNextWindowPos(ImVec2(300, 205), ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize({350, 285}, ImGuiSetCond_FirstUseEver);  // We don't want to set it *every* time, the user might have preferences
    if (milton_state->viz_window_visible)
    {
        bool opened = true;
        if (ImGui::Begin("Debug Data ([BACKQUOTE] to toggle)", &opened, ImGuiWindowFlags_NoCollapse))
        {
            float graph_height = 20;
            char msg[512] = {};

            float poll     = perf_count_to_sec(milton_state->graph_frame.polling) * 1000.0f;
            float update   = perf_count_to_sec(milton_state->graph_frame.update) * 1000.0f;
            float clipping = perf_count_to_sec(milton_state->graph_frame.clipping) * 1000.0f;
            {
                clipping /= milton_state->num_render_workers;
            }
            float raster   = perf_count_to_sec(milton_state->graph_frame.raster) * 1000.0f;
            float GL       = perf_count_to_sec(milton_state->graph_frame.GL) * 1000.0f;
            float system   = perf_count_to_sec(milton_state->graph_frame.system) * 1000.0f;

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
                     "Raster %f ms\n",
                     raster);
            ImGui::Text(msg);
            ImGui::SameLine();
            snprintf(msg, array_count(msg),
                     "(Clipping %f ms)\n",
                     clipping);
            ImGui::Text(msg);


            snprintf(msg, array_count(msg),
                     "OpenGL commands %f ms\n",
                     GL);
            ImGui::Text(msg);

            snprintf(msg, array_count(msg),
                     "System %f ms\n",
                     system);
            ImGui::Text(msg);

            float hist[] = { poll, update, raster, GL, system };
            ImGui::PlotHistogram("Graph",
                          (const float*)hist, array_count(hist));

            snprintf(msg, array_count(msg),
                     "Total %f ms\n",
                     sum);
            ImGui::Text(msg);

            ImGui::Dummy({0,30});

            i64 stroke_count = count_strokes(milton_state->root_layer);

            snprintf(msg, array_count(msg),
                     "# of strokes: %d (clipped to screen: %d)\n",
                     (int)stroke_count,
                     (int)count_clipped_strokes(milton_state->root_layer, milton_state->num_render_workers));
            ImGui::Text(msg);

            auto* view = milton_state->view;
            int screen_height = view->screen_size.h * view->scale;
            int screen_width = view->screen_size.w * view->scale;

            if (screen_height>0 && screen_height>0)
            {
                v2i pan = view->pan_vector;

                auto radius = view->canvas_radius_limit;

                {
                    if (pan.y < 0)
                    {
                        long n_screens_below = ((long)(radius) + (long)pan.y)/(long)screen_height;
                        snprintf(msg, array_count(msg),
                                 "Screens below: %ld\n", n_screens_below);
                        ImGui::Text(msg);
                    }
                    else
                    {
                        long n_screens_above = ((long)(radius) - (long)pan.y)/(long)screen_height;
                        snprintf(msg, array_count(msg),
                                 "Screens above: %ld\n", n_screens_above);
                        ImGui::Text(msg);
                    }
                }
                {
                    if (pan.x < 0)
                    {
                        long n_screens_below = ((long)(radius) + (long)pan.x)/(long)screen_width;
                        snprintf(msg, array_count(msg),
                                 "Screens to the right: %ld\n", n_screens_below);
                        ImGui::Text(msg);
                    }
                    else
                    {
                        long n_screens_above = ((long)(radius) - (long)pan.x)/(long)screen_width;
                        snprintf(msg, array_count(msg),
                                 "Screens to the left: %ld\n", n_screens_above);
                        ImGui::Text(msg);
                    }
                }
            }
            snprintf(msg, array_count(msg),
                     "Scale: %d", view->scale);
            ImGui::Text(msg);

            // Average stroke size.
            i64 avg = 0;
            {
                for(Layer* l = milton_state->root_layer;
                    l != NULL;
                    l = l->next)
                {
                    for (size_t i = 0; i < l->strokes.count; ++i)
                    {
                        Stroke* s = l->strokes.data + i;
                        avg += s->num_points;
                    }
                }
                if (stroke_count > 0)
                {
                    avg /= stroke_count;
                }
            }
            snprintf(msg, array_count(msg),
                     "Average stroke size: %" PRIi64, avg);
            ImGui::Text(msg);

        } ImGui::End();
    }
#endif

    // Shortcut help. Also shown regardless of UI visibility.
    ImGui::PopStyleColor(color_stack);
}

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
    picker->data.hsv.h = radians_to_degrees(angle);
    // Update the triangle
    float radius = 0.9f * (picker->wheel_radius - picker->wheel_half_width);
    v2f center = v2i_to_v2f(picker->center);
    {
        v2f point = polar_to_cartesian(-angle, radius);
        point = add2f(point, center);
        picker->data.c = point;
    }
    {
        v2f point = polar_to_cartesian(-angle + 2 * kPi / 3.0f, radius);
        point = add2f(point, center);
        picker->data.b = point;
    }
    {
        v2f point = polar_to_cartesian(-angle + 4 * kPi / 3.0f, radius);
        point = add2f(point, center);
        picker->data.a = point;
    }
}


static void picker_update_wheel(ColorPicker* picker, v2f polar_point)
{
    float angle = picker_wheel_get_angle(picker, polar_point);
    picker_update_points(picker, angle);
}

static b32 picker_hits_triangle(ColorPicker* picker, v2f fpoint)
{
    b32 result = is_inside_triangle(fpoint, picker->data.a, picker->data.b, picker->data.c);
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
    while (button)
    {
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
    if ((picker->flags & ColorPickerFlags_WHEEL_ACTIVE))
    {
        return true;
    }
    else if (is_inside_picker_rect(picker, point))
    {
        //return is_inside_picker_active_area(picker, point);
        return true;
    }
    else
    {
        return is_inside_picker_button_area(picker, point);
    }
}

static void picker_from_rgb(ColorPicker* picker, v3f rgb)
{
    v3f hsv = rgb_to_hsv(rgb);
    picker->data.hsv = hsv;
    float angle = hsv.h * 2*kPi;
    picker_update_points(picker, angle);
}

static b32 picker_hit_history_buttons(ColorPicker* picker, v2i point)
{
    b32 hits = false;
    ColorButton* first = &picker->color_buttons;
    ColorButton* button = first;
    while (button)
    {
        if ( button->rgba.a != 0 &&
             is_inside_rect(color_button_as_rect(button), point) )
        {
            hits = true;
            //picker->data = button->picker_data;
            picker_from_rgb(picker, button->rgba.rgb);
            v4f swp_color = button->rgba;
            button->rgba = first->rgba;
            first->rgba = swp_color;

            break;
        }
        button = button->next;
    }
    return hits;
}

static ColorPickResult picker_update(ColorPicker* picker, v2i point)
{
    ColorPickResult result = ColorPickResult_NOTHING;
    v2f fpoint = v2i_to_v2f(point);
    if (picker->flags == ColorPickerFlags_NOTHING)
    {
        if (picker_hits_wheel(picker, fpoint))
        {
            picker->flags |= ColorPickerFlags_WHEEL_ACTIVE;
        }
    }
    if ((picker->flags & ColorPickerFlags_WHEEL_ACTIVE))
    {
        if (!(picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE))
        {
            picker_update_wheel(picker, fpoint);
            result = ColorPickResult_CHANGE_COLOR;
        }
    }
    if (picker_hits_triangle(picker, fpoint))
    {
        if (!(picker->flags & ColorPickerFlags_WHEEL_ACTIVE))
        {
            picker->flags |= ColorPickerFlags_TRIANGLE_ACTIVE;
            picker->data.hsv = picker_hsv_from_point(picker, fpoint);
            result = ColorPickResult_CHANGE_COLOR;
        }
    }
    else if ((picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE))
    {
        // We don't want the chooser to "stick" if go outside the triangle
        // (i.e. picking black should be easy)
        v2f segments[] =
        {
            picker->data.a, picker->data.b,
            picker->data.b, picker->data.c,
            picker->data.c, picker->data.a,
        };

        for (i32 segment_i = 0; segment_i < 6; segment_i += 2)
        {
            v2i a = v2f_to_v2i(segments[segment_i    ]);
            v2i b = v2f_to_v2i(segments[segment_i + 1]);
            v2f intersection = { 0 };
            b32 hit = intersect_line_segments(point, picker->center,
                                              a, b,
                                              &intersection);
            if (hit)
            {
                picker->data.hsv = picker_hsv_from_point(picker, intersection);
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
    if (point.y > center.y)
    {
        angle = (2 * kPi) - angle;
    }
    return angle;
}

void picker_init(ColorPicker* picker)
{
    v2f fpoint =
    {
        (f32)picker->center.x + (int)(picker->wheel_radius),
        (f32)picker->center.y
    };
    picker_update_wheel(picker, fpoint);
    picker->data.hsv = v3f{ 0, 1, 1 };
}

static b32 picker_is_active(ColorPicker* picker)
{
    b32 is_active = (picker->flags & ColorPickerFlags_WHEEL_ACTIVE) || (picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE);

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
    mlt_assert (picker_rect.left >= 0);
    mlt_assert (picker_rect.top >= 0);

    return picker_rect;
}

void eyedropper_input(MiltonGui* gui, u32* canvas_buffer, i32 w, i32 h, v2i point)
{
    v4f color = color_u32_to_v4f(canvas_buffer[point.y*w+point.x]);

    picker_from_rgb(&gui->picker, color.rgb);
}

void exporter_init(Exporter* exporter)
{
    *exporter = Exporter{};
    exporter->scale = 1;
}

b32 exporter_input(Exporter* exporter, MiltonInput* input)
{
    b32 changed = false;
    if (input->input_count > 0)
    {
        changed = true;
        v2i point = input->points[input->input_count - 1];
        if ( exporter->state == ExporterState_EMPTY ||
             exporter->state == ExporterState_SELECTED )
        {
            exporter->pivot = point;
            exporter->needle = point;
            exporter->state = ExporterState_GROWING_RECT;
        }
        else if (exporter->state == ExporterState_GROWING_RECT)
        {
            exporter->needle = point;
        }
    }
    if ((input->flags & MiltonInputFlags_END_STROKE) && exporter->state != ExporterState_EMPTY)
    {
        exporter->state = ExporterState_SELECTED;
        changed = true;
    }
    return changed;
}

void gui_imgui_set_ungrabbed(MiltonGui* gui)
{
    gui->flags &= ~MiltonGuiFlags_SHOWING_PREVIEW;
}

Rect get_bounds_for_picker_and_colors(ColorPicker* picker)
{
    Rect result = rect_union(picker_get_bounds(picker), picker_color_buttons_bounds(picker));
    return result;
}


v3f picker_hsv_from_point(ColorPicker* picker, v2f point)
{
    float area = orientation(picker->data.a, picker->data.b, picker->data.c);
    v3f hsv = {0};
    if (area != 0)
    {
        float inv_area = 1.0f / area;
        float s = orientation(picker->data.b, point, picker->data.a) * inv_area;
        if (s > 1) { s = 1; }
        if (s < 0) { s = 0; }
        float v = 1 - (orientation(point, picker->data.c, picker->data.a) * inv_area);
        if (v > 1) { v = 1; }
        if (v < 0) { v = 0; }

        hsv = v3f
        {
            picker->data.hsv.h,
            s,
            v,
        };
    }
    return hsv;
}

v3f gui_get_picker_rgb(MiltonGui* gui)
{
    v3f rgb = hsv_to_rgb(gui->picker.data.hsv);
    return rgb;
}

// Returns true if the Picker consumed input. False if the GUI wasn't affected
b32 picker_consume_input(MiltonGui* gui, MiltonInput* input)
{
    b32 accepts = false;
    v2i point = input->points[0];
    if ( gui->visible )
    {
        accepts = picker_is_accepting_input(&gui->picker, point);
        if (!gui->did_hit_button &&
            !picker_is_active(&gui->picker) &&
            picker_hit_history_buttons(&gui->picker, point))
        {
            accepts = true;
            gui->did_hit_button = true;
        }
    }
    return accepts;
}

int gui_process_input(MiltonState* milton_state, MiltonInput* input)
{
    int render_flags = MiltonRenderFlags_NONE;
    v2i point = input->points[0];
    ColorPickResult pick_result = picker_update(&milton_state->gui->picker, point);
    if ( pick_result == ColorPickResult_CHANGE_COLOR &&
         milton_state->current_mode == MiltonMode_PEN )
    {
        /* v3f rgb = hsv_to_rgb(milton_state->gui->picker.data.hsv); */
        /* milton_state->brushes[BrushEnum_PEN].color = to_premultiplied(rgb, milton_state->brushes[BrushEnum_PEN].alpha); */
    }

    render_flags |= MiltonRenderFlags_UI_UPDATED;
    milton_state->gui->active = true;


    return render_flags;
}

void gui_toggle_visibility(MiltonState* milton_state)
{
    MiltonGui* gui = milton_state->gui;
    gui->flags |= MiltonGuiFlags_NEEDS_REDRAW;
    gui->visible = !gui->visible;
    if (gui->visible)
    {
        milton_state->render_data->flags |= RenderDataFlags_GUI_VISIBLE;
    }
    else
    {
        milton_state->render_data->flags &= ~RenderDataFlags_GUI_VISIBLE;
    }
}

void gui_toggle_help(MiltonGui* gui)
{
    gui->show_help_widget = !gui->show_help_widget;
}

void gui_init(Arena* root_arena, MiltonGui* gui)
{
    i32 bounds_radius_px = 100;
    f32 wheel_half_width = 12;
    gui->picker.center = v2i{ bounds_radius_px + 20, bounds_radius_px + 30 };
    gui->picker.bounds_radius_px = bounds_radius_px;
    gui->picker.wheel_half_width = wheel_half_width;
    gui->picker.wheel_radius = (f32)bounds_radius_px - 5.0f - wheel_half_width;
    gui->picker.data.hsv = v3f{ 0.0f, 1.0f, 0.7f };
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
    for (i32 i = 0; i < num_buttons; ++i)
    {
        mlt_assert (cur_button->next == NULL);

        cur_button->x = current_x;
        cur_button->y = gui->picker.center.y + bounds_radius_px + spacing;
        cur_button->w = button_size;
        cur_button->h = button_size;
        cur_button->rgba = {};

        current_x += spacing + button_size;

        if (i != (num_buttons - 1))
        {
            cur_button->next = arena_alloc_elem(root_arena, ColorButton);
        }
        cur_button = cur_button->next;
    }

    gui->preview_pos      = v2i{-1, -1};
    gui->preview_pos_prev = v2i{-1, -1};

    exporter_init(&gui->exporter);
}

// When a selected color is used in a stroke, call this to update the color
// button list.
b32 gui_mark_color_used(MiltonGui* gui)
{
    b32 changed = false;
    ColorButton* start = &gui->picker.color_buttons;
    v3f picker_color  = hsv_to_rgb(gui->picker.data.hsv);
    // Search for a color that is already in the list
    ColorButton* button = start;
    while(button)
    {
        if (button->rgba.a != 0)
        {
            v3f diff =
            {
                fabsf(button->rgba.r - picker_color.r),
                fabsf(button->rgba.g - picker_color.g),
                fabsf(button->rgba.b - picker_color.b),
            };
            float epsilon = 0.000001f;
            if (diff.r < epsilon && diff.g < epsilon && diff.b < epsilon)
            {
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
    if (!changed)
    {
        changed = true;
        v4f button_color = color_rgb_to_rgba(picker_color,1);
        // Pass data to the next one.
        while (button)
        {
            v4f tmp_color = button->rgba;
            button->rgba = button_color;
            button_color = tmp_color;
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

