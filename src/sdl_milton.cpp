// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "common.h"

#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include "gui.h"
#include "history_debugger.h"
#include "milton.h"
#include "platform.h"
#include "utils.h"


struct PlatformState
{
    i32 width;
    i32 height;

    b32 is_ctrl_down;
    b32 is_space_down;
    b32 is_pointer_down;  // Left click or wacom input
    b32 is_panning;
    v2i pan_start;
    v2i pan_point;

    b32 should_quit;
    u32 window_id;

    i32 num_pressure_results;
    i32 num_point_results;
    b32 stopped_panning;
};

MiltonInput sdl_event_loop(MiltonState* milton_state, PlatformState* platform_state)
{
    MiltonInput milton_input = {};

    v2i input_point = {};

    platform_state->num_pressure_results = 0;
    platform_state->num_point_results = 0;
    platform_state->stopped_panning = false;

    i32 input_flags = (i32)MiltonInputFlags_NONE;

    SDL_Event event;
    while ( SDL_PollEvent(&event) ) {
        //ImGui_ImplSdl_ProcessEvent(&event);
        ImGui_ImplSdlGL3_ProcessEvent(&event);

        SDL_Keymod keymod = SDL_GetModState();
        platform_state->is_ctrl_down = (keymod & KMOD_LCTRL) | (keymod & KMOD_RCTRL);


#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4061)
#endif
        switch ( event.type ) {
        case SDL_QUIT:
            platform_state->should_quit = true;
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
                // SDL does not implement this in the version we're using.
                // See platform_OSX_SDL_hooks.(h|m) for our SDL hack.
                break;
#endif
            default:
                break;  // Are we in Wayland yet?

            }
            if (er == EASYTAB_OK) {  // Event was handled.
                if (EasyTab->Pressure > 0) {
                    platform_state->is_pointer_down = true;
                    milton_input.points[platform_state->num_point_results++] = { EasyTab->PosX, EasyTab->PosY };
                    milton_input.pressures[platform_state->num_pressure_results++] = EasyTab->Pressure;
                }
            }
        } break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.windowID != platform_state->window_id) {
                break;
            }
            if ( event.button.button == SDL_BUTTON_LEFT ) {
                platform_state->is_pointer_down = true;
                if ( platform_state->is_panning ) {
                    platform_state->pan_start = { event.button.x, event.button.y };
                    platform_state->pan_point = platform_state->pan_start;  // No huge pan_delta at beginning of pan.
                }
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.windowID != platform_state->window_id) {
                break;
            }
            if (event.button.button == SDL_BUTTON_LEFT) {
                // Add final point
                if ( !platform_state->is_panning && platform_state->is_pointer_down ) {
                    set_flag(input_flags, MiltonInputFlags_END_STROKE);
                    input_point = { event.button.x, event.button.y };
                    milton_input.points[platform_state->num_point_results++] = input_point;
                    // Start drawing hover as soon as we stop the stroke.
                    milton_input.hover_point = input_point;
                    set_flag(input_flags, MiltonInputFlags_HOVERING);
                }
                else if ( platform_state->is_panning ) {
                    if (!platform_state->is_space_down) {
                        platform_state->is_panning = false;
                        platform_state->stopped_panning = true;
                    }
                }
                platform_state->is_pointer_down = false;
            }
            break;
        case SDL_MOUSEMOTION:
            {
                if (event.motion.windowID != platform_state->window_id) {
                    break;
                }
                input_point = { event.motion.x, event.motion.y };
                if (platform_state->is_pointer_down) {
                    if ( !platform_state->is_panning ) {
                        milton_input.points[platform_state->num_point_results++] = input_point;
                    }
                    else if ( platform_state->is_panning ) {
                        platform_state->pan_point = input_point;
                    }
                    unset_flag(input_flags, MiltonInputFlags_HOVERING);
                } else {
                    set_flag(input_flags, MiltonInputFlags_HOVERING);
                    milton_input.hover_point = input_point;
                }
                break;
            }
        case SDL_MOUSEWHEEL:
            {
                if (event.wheel.windowID != platform_state->window_id) {
                    break;
                }
                if ( !ImGui::GetIO().WantCaptureMouse ) {
                    milton_input.scale += event.wheel.y;
                    set_flag(input_flags, MiltonInputFlags_FAST_DRAW);
                }

                break;
            }
        case SDL_KEYDOWN:
            {
                if ( event.wheel.windowID != platform_state->window_id ) {
                    break;
                }

                SDL_Keycode keycode = event.key.keysym.sym;

                // Actions accepting key repeats.
                {
                    if ( keycode == SDLK_LEFTBRACKET ) {
                        milton_decrease_brush_size(milton_state);
                    }
                    else if ( keycode == SDLK_RIGHTBRACKET ) {
                        milton_increase_brush_size(milton_state);
                    }
                    if ( platform_state->is_ctrl_down ) {
                        if (keycode == SDLK_z) {
                            set_flag(input_flags, MiltonInputFlags_UNDO);
                        }
                        if (keycode == SDLK_y)
                        {
                            set_flag(input_flags, MiltonInputFlags_REDO);
                        }
                    }

                }
                if (event.key.repeat) {
                    break;
                }

                // Stop stroking when any key is hit
                platform_state->is_pointer_down = false;
                set_flag(input_flags, MiltonInputFlags_END_STROKE);

                if (keycode == SDLK_SPACE) {
                    platform_state->is_space_down = true;
                    platform_state->is_panning = true;
                    // Stahp
                }
                if ( platform_state->is_ctrl_down ) {  // Ctrl-KEY with no key repeats.
                    if ( keycode == SDLK_e ) {
                        set_flag(input_flags, MiltonInputFlags_CHANGE_MODE);
                        milton_input.mode_to_set = MiltonMode_EXPORTING;
                    }
                } else {
                    if ( !ImGui::GetIO().WantCaptureMouse ) {
                        if (keycode == SDLK_e) {
                            set_flag(input_flags, MiltonInputFlags_CHANGE_MODE);
                            milton_input.mode_to_set = MiltonMode_ERASER;
                        } else if (keycode == SDLK_b) {
                            set_flag(input_flags, MiltonInputFlags_CHANGE_MODE);
                            milton_input.mode_to_set = MiltonMode_PEN;
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
                    }
#if MILTON_DEBUG
                    if (keycode == SDLK_ESCAPE) {
                        platform_state->should_quit = true;
                    }
                    if ( keycode == SDLK_F4 ) {
                        milton_state->DEBUG_sse2_switch = !milton_state->DEBUG_sse2_switch;
                    }
#endif
                }

                break;
            }
        case SDL_KEYUP: {
            if (event.key.windowID != platform_state->window_id) {
                break;
            }

            SDL_Keycode keycode = event.key.keysym.sym;

            if ( keycode == SDLK_SPACE ) {
                platform_state->is_space_down = false;
                if ( !platform_state->is_pointer_down ) {
                    platform_state->is_panning = false;
                    platform_state->stopped_panning = true;
                }
            }
        } break;
        case SDL_WINDOWEVENT: {
            if ( platform_state->window_id != event.window.windowID ) {
                break;
            }
            switch ( event.window.event ) {
                // Just handle every event that changes the window size.
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                platform_state->width = event.window.data1;
                platform_state->height = event.window.data2;
                set_flag(input_flags, MiltonInputFlags_FULL_REFRESH);
                glViewport(0, 0, platform_state->width, platform_state->height);
                break;
            case SDL_WINDOWEVENT_LEAVE:
                if ( event.window.windowID != platform_state->window_id ) {
                    break;
                }
                if ( platform_state->is_pointer_down ) {
                    platform_state->is_pointer_down = false;
                    set_flag(input_flags, MiltonInputFlags_END_STROKE);
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
#if defined(_MSC_VER)
#pragma warning (pop)
#endif
        if ( platform_state->should_quit ) {
            break;
        }
    }  // ---- End of SDL event loop
    milton_input.flags = (MiltonInputFlags)input_flags;
    return milton_input;
}

// ---- milton_main

int milton_main(MiltonStartupFlags startup_flags)
{
    // Note: Possible crash regarding SDL_main entry point.
    // Note: Event handling, File I/O and Threading are initialized by default
    SDL_Init(SDL_INIT_VIDEO);

    PlatformState platform_state = {};

    platform_state.width = 1280;
    platform_state.height = 800;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_Window* window = SDL_CreateWindow("Milton",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          platform_state.width, platform_state.height,
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
    //ImGui_ImplSdl_Init(window);
    ImGui_ImplSdlGL3_Init(window);


    // ==== Initialize milton
    //  Total (static) memory requirement for Milton
    // TODO: Calculate how much is the arena using before shipping.
    size_t sz_root_arena = (size_t)1 * 1024 * 1024 * 1024;

    // Using platform_allocate because stdlib calloc will be really slow.
    void* big_chunk_of_memory = platform_allocate(sz_root_arena);

    if ( !big_chunk_of_memory ) {
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

#if defined(_MSC_VER)
#pragma warning (push, 0)
#endif
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
            break;
        }
    } else {
        milton_die_gracefully("Can't get system info!\n");
    }
#if defined(_MSC_VER)
#pragma warning (pop)
#endif

    milton_resize(milton_state, {}, {platform_state.width, platform_state.height});

    platform_state.window_id = SDL_GetWindowID(window);

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

    // ImGui setup
    {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;  // Don't save any imgui.ini file
    }

    // ---- Main loop ----

    if ( startup_flags.history_debug == HistoryDebug_REPLAY ) {

    }
    else while( !platform_state.should_quit ) {
        ImGuiIO& imgui_io = ImGui::GetIO();

        MiltonInput milton_input = sdl_event_loop(milton_state, &platform_state);

        // TODO: Is a static the best choice?
        static b32 first_run = true;
        if ( first_run ) {
            first_run = false;
            milton_input.flags = MiltonInputFlags_FULL_REFRESH;
        }

        if ( platform_state.stopped_panning ) {
            milton_state->request_quality_redraw = true;
        }

        /* if ( platform_state.is_pointer_down && check_flag(milton_input.flags, MiltonInputFlags_HOVERING) ) { */
        /*     milton_input.hover_point = {-100, -100}; */
        /* } */

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

        i32 input_flags = (i32)milton_input.flags;

        ImGui_ImplSdlGL3_NewFrame();
        // Clear our pointer input because we captured an ImGui widget!
        if (ImGui::GetIO().WantCaptureMouse) {
            platform_state.num_point_results = 0;
            platform_state.is_pointer_down = false;
            set_flag(input_flags, MiltonInputFlags_IMGUI_GRABBED_INPUT);
        }

        milton_gui_tick(&milton_input, milton_state);

        // Clear pan delta if we are zooming
        if ( milton_input.scale != 0 ) {
            milton_input.pan_delta = {};
        } else {
            if (platform_state.is_panning) {
                set_flag(input_flags, MiltonInputFlags_PANNING);
                platform_state.num_point_results = 0;
            }
        }

#if 0
        milton_log ("#Pressure results: %d\n", num_pressure_results);
        milton_log ("#   Point results: %d\n", num_point_results);
#endif

        // Mouse input, fill with NO_PRESSURE_INFO
        if ( platform_state.num_pressure_results == 0 ) {
            for (int i = platform_state.num_pressure_results; i < platform_state.num_point_results; ++i) {
                milton_input.pressures[platform_state.num_pressure_results++] = NO_PRESSURE_INFO;
            }
        }

        if ( platform_state.num_pressure_results < platform_state.num_point_results ) {
            platform_state.num_point_results = platform_state.num_pressure_results;
        }

        // Re-cast... This is dumb
        milton_input.flags = (MiltonInputFlags)( input_flags | (int)milton_input.flags );

        assert (platform_state.num_point_results <= platform_state.num_pressure_results);

        milton_input.input_count = platform_state.num_point_results;

        v2i pan_delta = sub2i(platform_state.pan_point, platform_state.pan_start);
        if ( pan_delta.x != 0 ||
             pan_delta.y != 0 ||
             platform_state.width != milton_state->view->screen_size.x ||
             platform_state.height != milton_state->view->screen_size.y ) {
            milton_resize(milton_state, pan_delta, {platform_state.width, platform_state.height});
        }
        milton_input.pan_delta = pan_delta;


        // ---- Recording. No more messing with milton_input after this.

        if ( startup_flags.history_debug == HistoryDebug_RECORD ) {
            history_debugger_append(&milton_input);
        }

        platform_state.pan_start = platform_state.pan_point;
        // ==== Update and render
        milton_update(milton_state, &milton_input);
        if ( !milton_state->running ) {
            platform_state.should_quit = true;
        }
        milton_gl_backend_draw(milton_state);
        ImGui::Render();
        SDL_GL_SwapWindow(window);
        SDL_WaitEvent(NULL);
    }


    // Release pages. Not really necessary but we don't want to piss off leak
    // detectors, do we?
    platform_deallocate(big_chunk_of_memory);

    SDL_Quit();

    return 0;
}

