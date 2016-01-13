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
#include <imgui_impl_sdl_gl3.h>

struct PlatformInput {
    b32 is_ctrl_down;
    b32 is_space_down;
    b32 is_pointer_down;  // Left click or wacom input
    b32 is_panning;
    v2i pan_start;
    v2i pan_point;
};

int milton_main()
{
    // Note: Possible crash regarding SDL_main entry point.
    // Note: Event handling, File I/O and Threading are initialized by default
    SDL_Init(SDL_INIT_VIDEO);


    i32 width = 1280;
    i32 height = 800;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);

    SDL_Window* window = SDL_CreateWindow("Milton",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          width, height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window) {
        milton_log("[ERROR] -- Exiting. SDL could not create window\n");
        exit(EXIT_FAILURE);
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if (!gl_context) {
        milton_fatal("Could not create OpenGL context\n");
    }

    platform_load_gl_func_pointers();

    milton_log("Created OpenGL context with version %s\n", glGetString(GL_VERSION));
    milton_log("    and GLSL %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Init ImGUI
    ImGui_ImplSDLGL3_Init();


    // ==== Initialize milton
    //  Total (static) memory requirement for Milton
    // TODO: Calculate how much is the arena using before shipping.
    size_t sz_root_arena = (size_t)1 * 1024 * 1024 * 1024;

    // Using platform_allocate because stdlib calloc will be really slow.
    void* big_chunk_of_memory = platform_allocate(sz_root_arena);

    if (!big_chunk_of_memory) {
        milton_fatal("Could allocate virtual memory for Milton.");
    }

    Arena root_arena = arena_init(big_chunk_of_memory, sz_root_arena);

    MiltonState* milton_state = arena_alloc_elem(&root_arena, MiltonState);
    {
        milton_state->root_arena = &root_arena;

        milton_init(milton_state);
    }

    // Ask for native events to poll tablet events.
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

    SDL_SysWMinfo sysinfo;
    SDL_VERSION(&sysinfo.version);
    if ( SDL_GetWindowWMInfo( window, &sysinfo ) ) {
        switch(sysinfo.subsystem) {
#if defined(_WIN32)
        case SDL_SYSWM_WINDOWS:
            EasyTab_Load(sysinfo.info.win.window);
            break;
#elif defined(__linux__)
        case SDL_SYSWM_X11:
            EasyTab_Load(sysinfo.info.x11.display, sysinfo.info.x11.window);
            break;
#endif
        default:
            // TODO Easytab port
            // milton_die_gracefully("Runtime system not recognized.");
            break;
        }
    } else {
        milton_die_gracefully("Can't get system info!\n");
    }


    PlatformInput platform_input = { 0 };

    b32 should_quit = false;
    MiltonInput milton_input = {};
    milton_input.flags = MiltonInputFlags::FULL_REFRESH;  // Full draw on first launch
    milton_resize(milton_state, {}, {width, height});

    u32 window_id = SDL_GetWindowID(window);
    v2i input_point = { 0 };

    // Every 100ms, call this callback to send us an event so we don't wait for user input.
    // Called periodically to force updates that don't depend on user input.
    SDL_AddTimer(100,
                 [](u32 interval, void *param) {
                     SDL_Event event;
                     SDL_UserEvent userevent;

                     userevent.type = SDL_USEREVENT;
                     userevent.code = 0;
                     userevent.data1 = NULL;
                     userevent.data2 = NULL;

                     event.type = SDL_USEREVENT;
                     event.user = userevent;

                     SDL_PushEvent(&event);
                     return(interval);
                 }, NULL);

    // IMGUI HELLO WORLD SHIT
    float f = 0.0f;
    bool show_test_window = true;
    bool show_another_window = false;

    while(!should_quit) {
        // ==== Handle events
        ImGuiIO& imgui_io = ImGui::GetIO();

        i32 num_pressure_results = 0;
        i32 num_point_results = 0;

        SDL_Event event;
        b32 got_tablet_point_input = false;
        while ( SDL_PollEvent(&event) ) {
            SDL_Keymod keymod = SDL_GetModState();
            platform_input.is_ctrl_down = (keymod & KMOD_LCTRL) | (keymod & KMOD_RCTRL);

            switch ( event.type ) {
            case SDL_QUIT:
                should_quit = true;
                break;
            case SDL_SYSWMEVENT: {
                f32 pressure = NO_PRESSURE_INFO;
                v2i point = { 0 };
                SDL_SysWMEvent sysevent = event.syswm;
                EasyTabResult er = EASYTAB_EVENT_NOT_HANDLED;
                switch(sysevent.msg->subsystem) {
#if defined(_WIN32)
                case SDL_SYSWM_WINDOWS:
                    er = EasyTab_HandleEvent(sysevent.msg->msg.win.hwnd,
                                             sysevent.msg->msg.win.msg,
                                             sysevent.msg->msg.win.lParam,
                                             sysevent.msg->msg.win.wParam);
                    break;
#elif defined(__linux__)
                case SDL_SYSWM_X11:
                    er = EasyTab_HandleEvent(&sysevent.msg->msg.x11.event);
                    break;
#elif defined(__MACH__)
                case SDL_SYSWM_COCOA:
                    // TODO impl
                    break;
#endif
                default:
                    break;  // Are we in Wayland yet?

                }
                if (er == EASYTAB_OK) {  // Event was handled.
                    if (EasyTab->Pressure > 0) {
                        platform_input.is_pointer_down = true;
                        milton_input.points[num_point_results++] = { EasyTab->PosX, EasyTab->PosY };
                        milton_input.pressures[num_pressure_results++] = EasyTab->Pressure;
                    }
                }
            } break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.windowID != window_id) {
                    break;
                }
                if ( event.button.button == SDL_BUTTON_LEFT ) {
                    platform_input.is_pointer_down = true;
                    if ( platform_input.is_panning ) {
                        platform_input.pan_start = { event.button.x, event.button.y };
                    }
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.windowID != window_id) {
                    break;
                }
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Add final point
                    if (!platform_input.is_panning && platform_input.is_pointer_down) {
                        set_flag(milton_input.flags, MiltonInputFlags::END_STROKE);
                        input_point = { event.button.x, event.button.y };
                        milton_input.points[num_point_results++] = input_point;
                        // Start drawing hover as soon as we stop the stroke.
                        milton_input.hover_point = input_point;
                        set_flag(milton_input.flags, MiltonInputFlags::HOVERING);
                    } else if (platform_input.is_panning) {
                        if (!platform_input.is_space_down) {
                            platform_input.is_panning = false;
                        }
                    }
                    platform_input.is_pointer_down = false;
                }
                break;
            case SDL_MOUSEMOTION:
                {
                    if (event.motion.windowID != window_id) {
                        break;
                    }
                    input_point = { event.motion.x, event.motion.y };
                    if (platform_input.is_pointer_down) {
                        if ( !platform_input.is_panning && !got_tablet_point_input ) {
                            milton_input.points[num_point_results++] = input_point;
                        } else if ( platform_input.is_panning ) {
                            platform_input.pan_point = input_point;

                            set_flag(milton_input.flags, MiltonInputFlags::FAST_DRAW);
                            set_flag(milton_input.flags, MiltonInputFlags::FULL_REFRESH);
                        }
                        unset_flag(milton_input.flags, MiltonInputFlags::HOVERING);
                    } else {
                        set_flag(milton_input.flags, MiltonInputFlags::HOVERING);
                        milton_input.hover_point = input_point;
                    }
                    break;
                }
            case SDL_MOUSEWHEEL:
                {
                    if (event.wheel.windowID != window_id) {
                        break;
                    }

                    milton_input.scale += event.wheel.y;
                    set_flag(milton_input.flags, MiltonInputFlags::FAST_DRAW);

                    break;
                }
            case SDL_KEYDOWN:
                {
                    if ( event.wheel.windowID != window_id ) {
                        break;
                    }

                    SDL_Keycode keycode = event.key.keysym.sym;

                    // Actions accepting key repeats.
                    {
                        if ( keycode == SDLK_LEFTBRACKET ) {
                            milton_decrease_brush_size(milton_state);
                        } else if ( keycode == SDLK_RIGHTBRACKET ) {
                            milton_increase_brush_size(milton_state);
                        }
                        if ( platform_input.is_ctrl_down ) {
                            if (keycode == SDLK_z) {
                                set_flag(milton_input.flags, MiltonInputFlags::UNDO);
                            }
                            if (keycode == SDLK_y)
                            {
                                set_flag(milton_input.flags, MiltonInputFlags::REDO);
                            }
                        }
                    }

                    if (event.key.repeat) {
                        break;
                    }

                    // Stop stroking when any key is hit
                    platform_input.is_pointer_down = false;
                    set_flag(milton_input.flags, MiltonInputFlags::END_STROKE);

                    if (keycode == SDLK_SPACE) {
                        platform_input.is_space_down = true;
                        platform_input.is_panning = true;
                        // Stahp
                    }
                    if (platform_input.is_ctrl_down) {
                        if (keycode == SDLK_BACKSPACE) {
                            set_flag(milton_input.flags, MiltonInputFlags::RESET);
                        }
                    } else {
                        if (keycode == SDLK_e) {
                            set_flag(milton_input.flags, MiltonInputFlags::CHANGE_MODE);
                            milton_input.mode_to_set = MiltonMode::ERASER;
                        } else if (keycode == SDLK_b) {
                            set_flag(milton_input.flags, MiltonInputFlags::CHANGE_MODE);
                            milton_input.mode_to_set = MiltonMode::PEN;
                        } else if (keycode == SDLK_r) {
                            set_flag(milton_input.flags, MiltonInputFlags::CHANGE_MODE);
                            milton_input.mode_to_set = MiltonMode::EXPORTING;
                        } else if (keycode == SDLK_TAB) {
                            gui_toggle_visibility(milton_state->gui);
                        } else if (keycode == SDLK_F1) {
                            gui_toggle_help(milton_state->gui);
                        } else if (keycode == SDLK_1) {
                            milton_set_pen_alpha(milton_state, 0.1f);
                        } else if (keycode == SDLK_2) {
                            milton_set_pen_alpha(milton_state, 0.2f);
                        } else if (keycode == SDLK_3) {
                            milton_set_pen_alpha(milton_state, 0.3f);
                        } else if (keycode == SDLK_4) {
                            milton_set_pen_alpha(milton_state, 0.4f);
                        } else if (keycode == SDLK_5) {
                            milton_set_pen_alpha(milton_state, 0.5f);
                        } else if (keycode == SDLK_6) {
                            milton_set_pen_alpha(milton_state, 0.6f);
                        } else if (keycode == SDLK_7) {
                            milton_set_pen_alpha(milton_state, 0.7f);
                        } else if (keycode == SDLK_8) {
                            milton_set_pen_alpha(milton_state, 0.8f);
                        } else if (keycode == SDLK_9) {
                            milton_set_pen_alpha(milton_state, 0.9f);
                        } else if (keycode == SDLK_0) {
                            milton_set_pen_alpha(milton_state, 1.0f);
                        }
#ifndef NDEBUG
                        if (keycode == SDLK_ESCAPE) {
                            should_quit = true;
                        }
                        if ( keycode == SDLK_F4 ) {
                            milton_state->DEBUG_sse2_switch = !milton_state->DEBUG_sse2_switch;
                        }
#endif
                    }

                    break;
                }
            case SDL_KEYUP: {
                if (event.key.windowID != window_id) {
                    break;
                }

                SDL_Keycode keycode = event.key.keysym.sym;

                if (keycode == SDLK_SPACE)
                {
                    platform_input.is_space_down = false;
                    if (!platform_input.is_pointer_down)
                    {
                        platform_input.is_panning = false;
                    }
                }
            } break;
            case SDL_WINDOWEVENT: {
                if (window_id != event.window.windowID)
                {
                    break;
                }
                switch (event.window.event)
                {
                    // Just handle every event that changes the window size.
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    width = event.window.data1;
                    height = event.window.data2;
                    set_flag(milton_input.flags, MiltonInputFlags::FULL_REFRESH);
                    glViewport(0, 0, width, height);
                    break;
                case SDL_WINDOWEVENT_LEAVE:
                    if (event.window.windowID != window_id) {
                        break;
                    }
                    if (platform_input.is_pointer_down) {
                        platform_input.is_pointer_down = false;
                        set_flag(milton_input.flags, MiltonInputFlags::END_STROKE);
                    }
                    break;
                    // --- A couple of events we might want to catch later...
                case SDL_WINDOWEVENT_ENTER:
                    break;
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    break;
                default:
                    break;
                }
            } break;
            default:
                break;
            }
            if (should_quit) {
                break;
            }
        }

        if ( platform_input.is_pointer_down && check_flag(milton_input.flags, MiltonInputFlags::HOVERING) ) {
            milton_input.hover_point = {-100, -100};
        }
        // IN OSX: SDL polled all events, we get all the pressure inputs from our hook
#if defined(__MACH__)
        assert( num_pressure_results == 0 );
        int num_polled_pressures = 0;
        float* polled_pressures = milton_osx_poll_pressures(&num_polled_pressures);
        if ( num_polled_pressures ) {
            for (int i = num_polled_pressures - 1; i >= 0; --i) {
                milton_input.pressures[num_pressure_results++] = polled_pressures[i];
            }
        }
#endif


        // TODO: get framebuffer size
        ImGui_ImplSDLGL3_NewFrame(width, height, width, height);

        milton_gui_tick(milton_input.flags, milton_state);
        {
            int mouse_x;
            int mouse_y;
            SDL_GetMouseState(&mouse_x, &mouse_y);

            imgui_io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);

            // TODO: if focused, else -1,-1
            imgui_io.MouseDown[0] = (bool)(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT));
            imgui_io.MouseDown[1] = (bool)(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_MIDDLE));
            imgui_io.MouseDown[2] = (bool)(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT));

            // Clear our pointer input because we captured an ImGui widget!
            if (ImGui::GetIO().WantCaptureMouse) {
                num_point_results = 0;
                platform_input.is_pointer_down = false;
                set_flag(milton_input.flags, MiltonInputFlags::IMGUI_GRABBED_INPUT);
            }
        }

        if (platform_input.is_panning) {
            set_flag(milton_input.flags, MiltonInputFlags::PANNING);
            num_point_results = 0;
        }

#if 0
        milton_log ("#Pressure results: %d\n", num_pressure_results);
        milton_log ("#   Point results: %d\n", num_point_results);
#endif

        // Mouse input, fill with NO_PRESSURE_INFO
        if (num_pressure_results == 0 && !got_tablet_point_input) {
            for (int i = num_pressure_results; i < num_point_results; ++i) {
                milton_input.pressures[num_pressure_results++] = NO_PRESSURE_INFO;
            }
        }

        if (num_pressure_results < num_point_results) {
            num_point_results = num_pressure_results;
        }

        assert (num_point_results <= num_pressure_results);

        milton_input.input_count = num_point_results;

        v2i pan_delta = platform_input.pan_point - platform_input.pan_start;
        if ( pan_delta.x != 0 ||
             pan_delta.y != 0 ||
             width != milton_state->view->screen_size.x ||
             height != milton_state->view->screen_size.y ) {
              milton_resize(milton_state, pan_delta, {width, height});
        }
        milton_input.pan_delta = pan_delta;
        platform_input.pan_start = platform_input.pan_point;
        // ==== Update and render
        milton_update(milton_state, &milton_input);
        milton_gl_backend_draw(milton_state);
        ImGui::Render();
        SDL_GL_SwapWindow(window);
        SDL_WaitEvent(NULL);

        milton_input = {};
    }


    // Release pages. Not really necessary but we don't want to piss off leak
    // detectors, do we?
    platform_deallocate(big_chunk_of_memory);

    SDL_Quit();

    return 0;
}
