// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include "milton.h"
#include "gl_helpers.h"
#include "gui.h"
#include "persist.h"


static void
cursor_set_and_show(SDL_Cursor* cursor)
{
    SDL_SetCursor(cursor);
    platform_cursor_show();
}

LayoutType
get_current_keyboard_layout()
{
    LayoutType layout = LayoutType_QWERTY;  // Default to QWERTY bindings.

    char keys[] = {
        (char)SDL_GetKeyFromScancode(SDL_SCANCODE_Q),
        (char)SDL_GetKeyFromScancode(SDL_SCANCODE_R),
        (char)SDL_GetKeyFromScancode(SDL_SCANCODE_Y),
        '\0',
    };

    if ( strcmp(keys, "qry") == 0 ) {
        layout = LayoutType_QWERTY;
    }
    else if ( strcmp(keys, "ary") == 0 ) {
        layout = LayoutType_AZERTY;
    }
    else if ( strcmp(keys, "qrz") == 0 ) {
        layout = LayoutType_QWERTZ;
    }
    else if ( strcmp(keys, "q,f") ) {
        layout = LayoutType_DVORAK;
    }
    else if ( strcmp(keys, "qwj") == 0 ) {
        layout = LayoutType_COLEMAK;
    }

    return layout;
}

void
panning_update(PlatformState* platform_state)
{
    auto reset_pan_start = [platform_state]() {
        platform_state->pan_start = VEC2L(platform_state->pointer);
        platform_state->pan_point = platform_state->pan_start;  // No huge pan_delta at beginning of pan.
    };

    platform_state->was_panning = platform_state->is_panning;

    // Panning from GUI menu, waiting for input
    if ( platform_state->waiting_for_pan_input ) {
        if ( platform_state->is_pointer_down ) {
            platform_state->waiting_for_pan_input = false;
            platform_state->is_panning = true;
            reset_pan_start();
        }
        // Space cancels waiting
        if ( platform_state->is_space_down ) {
            platform_state->waiting_for_pan_input = false;
        }
    }
    else {
        if ( platform_state->is_panning ) {
            if ( (!platform_state->is_pointer_down && !platform_state->is_space_down)
                 || !platform_state->is_pointer_down ) {
                platform_state->is_panning = false;
            }
            else {
                platform_state->pan_point = VEC2L(platform_state->pointer);
            }
        }
        else {
            if ( (platform_state->is_space_down && platform_state->is_pointer_down)
                 || platform_state->is_middle_button_down ) {
                platform_state->is_panning = true;
                reset_pan_start();
            }
        }
    }
}

MiltonInput
sdl_event_loop(MiltonState* milton_state, PlatformState* platform_state)
{
    MiltonInput milton_input = {};
    milton_input.mode_to_set = MiltonMode::NONE;

    b32 pointer_up = false;

    v2i input_point = {};

    platform_state->num_pressure_results = 0;
    platform_state->num_point_results = 0;
    platform_state->keyboard_layout = get_current_keyboard_layout();

    i32 input_flags = (i32)MiltonInputFlags_NONE;

    SDL_Event event;
    while ( SDL_PollEvent(&event) ) {
        ImGui_ImplSdlGL3_ProcessEvent(&event);

        SDL_Keymod keymod = SDL_GetModState();
        platform_state->is_ctrl_down = (keymod & KMOD_LCTRL) | (keymod & KMOD_RCTRL);
        platform_state->is_shift_down = (keymod & KMOD_SHIFT);

#if 0
        if ( (keymod & KMOD_ALT) )
        {
            milton_input.mode_to_set = MiltonMode_EYEDROPPER;
        }
#endif


#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4061)
#endif
        switch ( event.type ) {
            case SDL_QUIT:
            platform_cursor_show();
            milton_try_quit(milton_state);
            break;
            case SDL_SYSWMEVENT: {
                f32 pressure = NO_PRESSURE_INFO;
                SDL_SysWMEvent sysevent = event.syswm;
                EasyTabResult er = EASYTAB_EVENT_NOT_HANDLED;
                if (!EasyTab) { break; }

                i32 bit_touch_old = (EasyTab->Buttons & EasyTab_Buttons_Pen_Touch);

                switch( sysevent.msg->subsystem ) {
#if defined(_WIN32)
                case SDL_SYSWM_WINDOWS: {

                    er = EasyTab_HandleEvent(sysevent.msg->msg.win.hwnd,
                                             sysevent.msg->msg.win.msg,
                                             sysevent.msg->msg.win.lParam,
                                             sysevent.msg->msg.win.wParam);

                } break;
#elif defined(__linux__)
                case SDL_SYSWM_X11:{
                    er = EasyTab_HandleEvent(&sysevent.msg->msg.x11.event);
                } break;
#elif defined(__MACH__)
                case SDL_SYSWM_COCOA:
                    // SDL does not implement this in the version we're using.
                    // See platform_OSX_SDL_hooks.(h|m) for our SDL hack.
                    break;
#endif
                default:
                    break;  // Are we in Wayland yet?

                }

                if ( er == EASYTAB_OK ) {
                    i32 bit_touch = (EasyTab->Buttons & EasyTab_Buttons_Pen_Touch);
                    i32 bit_lower = (EasyTab->Buttons & EasyTab_Buttons_Pen_Lower);
                    i32 bit_upper = (EasyTab->Buttons & EasyTab_Buttons_Pen_Upper);

                    // Pen in use but not drawing
                    b32 taking_pen_input = EasyTab->PenInProximity
                                           && bit_touch
                                           && !( bit_upper || bit_lower );

                    if ( taking_pen_input ) {
                        platform_state->is_pointer_down = true;

                        for ( int pi = 0; pi < EasyTab->NumPackets; ++pi ) {
                            v2l point = { EasyTab->PosX[pi], EasyTab->PosY[pi] };

                            platform_point_to_pixel(platform_state, &point);

                            if ( point.x >= 0 && point.y >= 0 ) {
                                if ( platform_state->num_point_results < MAX_INPUT_BUFFER_ELEMS ) {
                                    milton_input.points[platform_state->num_point_results++] = point;
                                }
                                if ( platform_state->num_pressure_results < MAX_INPUT_BUFFER_ELEMS ) {
                                    milton_input.pressures[platform_state->num_pressure_results++] = EasyTab->Pressure[pi];
                                }
                            }
                        }
                    }

                    if ( !bit_touch && bit_touch_old ) {
                        pointer_up = true;  // Wacom does not seem to send button-up messages after
                                            // using stylus buttons while stroking.
                    }


                    if ( EasyTab->NumPackets > 0 ) {
                        v2i point = { EasyTab->PosX[EasyTab->NumPackets-1], EasyTab->PosY[EasyTab->NumPackets-1] };

                        platform_point_to_pixel_i(platform_state, &point);

                        input_flags |= MiltonInputFlags_HOVERING;

                        platform_state->pointer = point;
                    }
                }
            } break;
            case SDL_MOUSEBUTTONDOWN: {
                if ( event.button.windowID != platform_state->window_id ) {
                    break;
                }

                if (   (event.button.button == SDL_BUTTON_LEFT && ( EasyTab == NULL || !EasyTab->PenInProximity))
                     || event.button.button == SDL_BUTTON_MIDDLE
                     // Ignoring right click events for now
                     /*|| event.button.button == SDL_BUTTON_RIGHT*/ ) {
                    if ( !ImGui::GetIO().WantCaptureMouse ) {
                        v2l long_point = { event.button.x, event.button.y };

                        platform_point_to_pixel(platform_state, &long_point);

                        v2i point = v2i{(int)long_point.x, (int)long_point.y};

                        if ( !platform_state->is_panning && point.x >= 0 && point.y > 0 ) {
                            input_flags |= MiltonInputFlags_CLICK;
                            milton_input.click = point;

                            platform_state->is_pointer_down = true;
                            platform_state->pointer = point;
                            platform_state->is_middle_button_down = (event.button.button == SDL_BUTTON_MIDDLE);

                            if ( platform_state->num_point_results < MAX_INPUT_BUFFER_ELEMS ) {
                                milton_input.points[platform_state->num_point_results++] = VEC2L(point);
                            }
                            if ( platform_state->num_pressure_results < MAX_INPUT_BUFFER_ELEMS ) {
                                milton_input.pressures[platform_state->num_pressure_results++] = NO_PRESSURE_INFO;
                            }
                        }
                    }
                    else {
                        platform_state->force_next_frame = true;
                    }
                }
            } break;
            case SDL_MOUSEBUTTONUP: {
                if ( event.button.windowID != platform_state->window_id ) {
                    break;
                }
                if ( event.button.button == SDL_BUTTON_LEFT
                     || event.button.button == SDL_BUTTON_MIDDLE
                     || event.button.button == SDL_BUTTON_RIGHT ) {
                    if ( event.button.button == SDL_BUTTON_MIDDLE ) {
                        platform_state->is_middle_button_down = false;
                    }
                    pointer_up = true;
                    input_flags |= MiltonInputFlags_CLICKUP;
                    input_flags |= MiltonInputFlags_END_STROKE;
                }
            } break;
            case SDL_MOUSEMOTION: {
                if (event.motion.windowID != platform_state->window_id) {
                    break;
                }
                input_point = {event.motion.x, event.motion.y};

                platform_point_to_pixel_i(platform_state, &input_point);

                platform_state->pointer = input_point;

                // In case the wacom driver craps out, or anything goes wrong (like the event queue
                // overflowing ;)) then we default to receiving WM_MOUSEMOVE. If we catch a single
                // point, then it's fine. It will get filtered out in milton_stroke_input

                if (EasyTab == NULL || !EasyTab->PenInProximity) {
                    if (platform_state->is_pointer_down) {
                        if (!platform_state->is_panning &&
                            (input_point.x >= 0 && input_point.y >= 0)) {
                            if (platform_state->num_point_results < MAX_INPUT_BUFFER_ELEMS) {
                                milton_input.points[platform_state->num_point_results++] = VEC2L(input_point);
                            }
                            if (platform_state->num_pressure_results < MAX_INPUT_BUFFER_ELEMS) {
                                milton_input.pressures[platform_state->num_pressure_results++] = NO_PRESSURE_INFO;
                            }
                        }
                        input_flags &= ~MiltonInputFlags_HOVERING;
                    } else {
                        input_flags |= MiltonInputFlags_HOVERING;
                    }
                }
                break;
            }
            case SDL_MOUSEWHEEL: {
                if ( event.wheel.windowID != platform_state->window_id ) {
                    break;
                }
                if ( !ImGui::GetIO().WantCaptureMouse ) {
                    milton_input.scale += event.wheel.y;
                    v2i zoom_center = platform_state->pointer;

                    milton_set_zoom_at_point(milton_state, zoom_center);
                    // ImGui has a delay of 1 frame when displaying zoom info.
                    // Force next frame to have the value up to date.
                    platform_state->force_next_frame = true;
                }

                break;
            }
            case SDL_KEYDOWN: {
                if ( event.wheel.windowID != platform_state->window_id ) {
                    break;
                }

                SDL_Keycode keycode = event.key.keysym.sym;

                // Actions accepting key repeats.
                {
                    if ( keycode == SDLK_LEFTBRACKET ) {
                        milton_decrease_brush_size(milton_state);
                        milton_state->hover_flash_ms = (i32)SDL_GetTicks();
                    }
                    else if ( keycode == SDLK_RIGHTBRACKET ) {
                        milton_increase_brush_size(milton_state);
                        milton_state->hover_flash_ms = (i32)SDL_GetTicks();
                    }
                    if ( platform_state->is_ctrl_down ) {
                        if ( (platform_state->keyboard_layout == LayoutType_QWERTZ && (keycode == SDLK_ASTERISK))
                             || (platform_state->keyboard_layout == LayoutType_AZERTY && (keycode == SDLK_EQUALS))
                             || (platform_state->keyboard_layout == LayoutType_QWERTY && (keycode == SDLK_EQUALS))
                             || keycode == SDLK_PLUS ) {
                            milton_input.scale++;
                            milton_set_zoom_at_screen_center(milton_state);
                        }
                        if ( (platform_state->keyboard_layout == LayoutType_AZERTY && (keycode == SDLK_6))
                             || keycode == SDLK_MINUS ) {
                            milton_input.scale--;
                            milton_set_zoom_at_screen_center(milton_state);
                        }
                        if ( keycode == SDLK_z ) {
                            if ( platform_state->is_shift_down ) {
                                input_flags |= MiltonInputFlags_REDO;
                            }
                            else {
                                input_flags |= MiltonInputFlags_UNDO;
                            }
                        }
                    }

                }

                if ( event.key.repeat ) {
                    break;
                }

                // Stop stroking when any key is hit
                input_flags |= MiltonInputFlags_END_STROKE;

                if ( keycode == SDLK_SPACE ) {
                    platform_state->is_space_down = true;
                    // Stahp
                }
                // Ctrl-KEY with no key repeats.
                if ( platform_state->is_ctrl_down ) {
                    if ( keycode == SDLK_e ) {
                        milton_input.mode_to_set = MiltonMode::EXPORTING;
                    }
                    if ( keycode == SDLK_q ) {
                        milton_try_quit(milton_state);
                    }
                    char* default_will_be_lost = "The default canvas will be cleared. Save it?";
                    if ( keycode == SDLK_n ) {
                        b32 save_file = false;
                        if ( layer::count_strokes(milton_state->canvas->root_layer) > 0 ) {
                            if ( milton_state->flags & MiltonStateFlags_DEFAULT_CANVAS ) {
                                save_file = platform_dialog_yesno(default_will_be_lost, "Save?");
                            }
                        }
                        if ( save_file ) {
                            PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                            if ( name ) {
                                milton_log("Saving to %s\n", name);
                                milton_set_canvas_file(milton_state, name);
                                milton_save(milton_state);
                                b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"), DeleteErrorTolerance_OK_NOT_EXIST);
                                if ( del == false ) {
                                    platform_dialog("Could not delete contents. The work will be still be there even though you saved it to a file.",
                                                    "Info");
                                }
                            }
                        }

                        // New Canvas
                        milton_reset_canvas_and_set_default(milton_state);
                        input_flags |= MiltonInputFlags_FULL_REFRESH;
                        milton_state->flags |= MiltonStateFlags_DEFAULT_CANVAS;

                    }
                    if ( keycode == SDLK_o ) {
                        b32 save_requested = false;
                        // If current canvas is MiltonPersist, then prompt to save
                        if ( ( milton_state->flags & MiltonStateFlags_DEFAULT_CANVAS ) ) {
                            b32 save_file = false;
                            if ( layer::count_strokes(milton_state->canvas->root_layer) > 0 ) {
                                save_file = platform_dialog_yesno(default_will_be_lost, "Save?");
                            }
                            if ( save_file ) {
                                PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                                if ( name ) {
                                    milton_log("Saving to %s\n", name);
                                    milton_set_canvas_file(milton_state, name);
                                    milton_save(milton_state);
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
                            milton_set_canvas_file(milton_state, fname);
                            input_flags |= MiltonInputFlags_OPEN_FILE;
                        }
                    }
                    if ( keycode == SDLK_a ) {
                        // NOTE(possible refactor): There is a copy of this at milton.c end of file
                        PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                        if ( name ) {
                            milton_log("Saving to %s\n", name);
                            milton_set_canvas_file(milton_state, name);
                            input_flags |= MiltonInputFlags_SAVE_FILE;
                            b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"),
                                                                     DeleteErrorTolerance_OK_NOT_EXIST);
                            if ( del == false ) {
                                platform_dialog("Could not delete default canvas. Contents will be still there when you create a new canvas.",
                                                "Info");
                            }
                        }
                    }
                }
                else {
                    if ( !ImGui::GetIO().WantCaptureMouse  ) {
                        if ( keycode == SDLK_m ) {
                            gui_toggle_menu_visibility(milton_state->gui);
                        }
                        else if ( keycode == SDLK_e ) {
                            milton_input.mode_to_set = MiltonMode::ERASER;
                        }
                        else if ( keycode == SDLK_b ) {
                            milton_input.mode_to_set = MiltonMode::PEN;
                        }
                        else if ( keycode == SDLK_i ) {
                            milton_input.mode_to_set = MiltonMode::EYEDROPPER;
                        }
                        else if ( keycode == SDLK_l ) {
                            milton_input.mode_to_set = MiltonMode::PRIMITIVE;
                        }
                        else if ( keycode == SDLK_TAB ) {
                            gui_toggle_visibility(milton_state);
                        }
                        else if ( keycode == SDLK_F1 ) {
                            gui_toggle_help(milton_state->gui);
                        }
                        else if ( keycode == SDLK_1 ) {
                            milton_set_pen_alpha(milton_state, 0.1f);
                        }
                        else if ( keycode == SDLK_2 ) {
                            milton_set_pen_alpha(milton_state, 0.2f);
                        }
                        else if ( keycode == SDLK_3 ) {
                            milton_set_pen_alpha(milton_state, 0.3f);
                        }
                        else if ( keycode == SDLK_4 ) {
                            milton_set_pen_alpha(milton_state, 0.4f);
                        }
                        else if ( keycode == SDLK_5 ) {
                            milton_set_pen_alpha(milton_state, 0.5f);
                        }
                        else if ( keycode == SDLK_6 ) {
                            milton_set_pen_alpha(milton_state, 0.6f);
                        }
                        else if ( keycode == SDLK_7 ) {
                            milton_set_pen_alpha(milton_state, 0.7f);
                        }
                        else if ( keycode == SDLK_8 ) {
                            milton_set_pen_alpha(milton_state, 0.8f);
                        }
                        else if ( keycode == SDLK_9 ) {
                            milton_set_pen_alpha(milton_state, 0.9f);
                        }
                        else if ( keycode == SDLK_0 ) {
                            milton_set_pen_alpha(milton_state, 1.0f);
                        }
                    }
#if MILTON_ENABLE_PROFILING
                    if ( keycode == SDLK_BACKQUOTE ) {
                        milton_state->viz_window_visible = !milton_state->viz_window_visible;
                    }
#endif
                }

                break;
            }
            case SDL_KEYUP: {
                if ( event.key.windowID != platform_state->window_id ) {
                    break;
                }

                SDL_Keycode keycode = event.key.keysym.sym;

                if ( keycode == SDLK_SPACE ) {
                    platform_state->is_space_down = false;
                }
            } break;
            case SDL_WINDOWEVENT: {
                if ( platform_state->window_id != event.window.windowID ) {
                    break;
                }
                switch ( event.window.event ) {
                    // Just handle every event that changes the window size.
                case SDL_WINDOWEVENT_MOVED:
                    platform_state->num_point_results = 0;
                    platform_state->num_pressure_results = 0;
                    platform_state->is_pointer_down = false;
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED: {

                    v2i size = { event.window.data1, event.window.data2 };
                    platform_point_to_pixel_i(platform_state, &size);

                    platform_state->width = size.w;
                    platform_state->height = size.h;


                    input_flags |= MiltonInputFlags_FULL_REFRESH;
                    glViewport(0, 0, platform_state->width, platform_state->height);
                    break;
                }
                case SDL_WINDOWEVENT_LEAVE:
                    if ( event.window.windowID != platform_state->window_id )
                    {
                        break;
                    }
                    platform_cursor_show();
                    break;
                    // --- A couple of events we might want to catch later...
                case SDL_WINDOWEVENT_ENTER:
                    {
                    } break;
                    break;
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    break;
                default:
                    break;
                }
            } break;
            default: {
                break;
            }
        }
#if defined(_MSC_VER)
#pragma warning (pop)
#endif
        if ( platform_state->should_quit ) {
            break;
        }
    }  // ---- End of SDL event loop

    if ( pointer_up ) {
        // Add final point
        if ( !platform_state->is_panning && platform_state->is_pointer_down ) {
            input_flags |= MiltonInputFlags_END_STROKE;
            input_point = { event.button.x, event.button.y };

            platform_point_to_pixel_i(platform_state, &input_point);

            if ( platform_state->num_point_results < MAX_INPUT_BUFFER_ELEMS ) {
                milton_input.points[platform_state->num_point_results++] = VEC2L(input_point);
            }
            // Start drawing hover as soon as we stop the stroke.
            input_flags |= MiltonInputFlags_HOVERING;
        }
        platform_state->is_pointer_down = false;

        platform_state->num_point_results = 0;
    }

    milton_input.flags = input_flags;

    return milton_input;
}

// ---- milton_main

int
milton_main(bool is_fullscreen, char* file_to_open)
{
    {
        static char* release_string
#if MILTON_DEBUG
                = "Debug";
#else
                = "Release";
#endif

        milton_log("Running Milton %d.%d.%d (%s) \n", MILTON_MAJOR_VERSION, MILTON_MINOR_VERSION, MILTON_MICRO_VERSION, release_string);
    }
    // Note: Possible crash regarding SDL_main entry point.
    // Note: Event handling, File I/O and Threading are initialized by default
    milton_log("Initializing SDL... ");
    SDL_Init(SDL_INIT_VIDEO);
    milton_log("Done.\n");

    #ifdef __linux__
    gtk_init(NULL, NULL);
    #endif

    PlatformState platform_state = {};

    PlatformPrefs prefs = {};

    milton_log("Loading preferences...\n");
    if ( milton_prefs_load(&prefs) ) {
        milton_log("Prefs file window size: %dx%d\n", prefs.width, prefs.height);
    }

    i32 window_width = 1280;
    i32 window_height = 800;
    {
        if (prefs.width > 0 && prefs.height > 0) {
            if ( !is_fullscreen ) {
                window_width = prefs.width;
                window_height = prefs.height;
            }
            else {
                // TODO: Does this work on retina mac?
                milton_log("Running fullscreen\n");
                SDL_DisplayMode dm;
                SDL_GetDesktopDisplayMode(0, &dm);

                window_width = dm.w;
                window_height = dm.h;
            }
        }
    }

    milton_log("Window dimensions: %dx%d \n", window_width, window_height);

#if defined(_WIN32)
    platform_state.win_dpi_api = (WinDpiApi*)mlt_calloc(1, sizeof(WinDpiApi), "Setup");
    win_load_dpi_api(platform_state.win_dpi_api);

    platform_state.win_dpi_api->SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#endif


    platform_state.ui_scale = 1.0f;

    platform_state.keyboard_layout = get_current_keyboard_layout();

#if USE_GL_3_2
    i32 gl_version_major = 3;
    i32 gl_version_minor = 2;
    milton_log("Requesting OpenGL 3.2 context.\n");
#else
    i32 gl_version_major = 2;
    i32 gl_version_minor = 1;
    milton_log("Requesting OpenGL 2.1 context.\n");
#endif

    SDL_Window* window = NULL;
    milton_log("Creating Milton Window\n");

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gl_version_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, gl_version_minor);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);
    #if USE_GL_3_2
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    #endif
    #if MILTON_DEBUG
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    #endif

    #if MULTISAMPLING_ENABLED
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, MSAA_NUM_SAMPLES);
    #endif

    Uint32 sdl_window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

    if (is_fullscreen) {
        sdl_window_flags |= SDL_WINDOW_FULLSCREEN;
    }
    else {
        sdl_window_flags |= SDL_WINDOW_RESIZABLE;
    }

    window = SDL_CreateWindow("Milton",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              window_width, window_height,
                              sdl_window_flags);

    if ( !window ) {
        milton_log("SDL Error: %s\n", SDL_GetError());
        milton_die_gracefully("SDL could not create window\n");
    }

    platform_state.window = window;

    // Milton works in pixels, but macOS works distinguishing "points" and
    // "pixels", with most APIs working in points.

    v2l size_px = { window_width, window_height };
    platform_point_to_pixel(&platform_state, &size_px);

    platform_state.width = size_px.w;
    platform_state.height = size_px.h;

    // Sometimes SDL sets the window position such that it's impossible to move
    // without using Windows shortcuts that not everyone knows. Check if this
    // is the case and set a good default.
    {
        if (!is_fullscreen) {
            int x = 0, y = 0;
            SDL_GetWindowPosition(window, &x, &y);
            if ( x < 0 && y < 0 ) {
                milton_log("Negative coordinates for window position. Setting it to 100,100. \n");
                SDL_SetWindowPosition(window, 100, 100);
            }
        }
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if ( !gl_context ) {
        milton_die_gracefully("Could not create OpenGL context\n");
    }

    SDL_GL_SetSwapInterval(0);

    int actual_major = 0;
    int actual_minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &actual_major);
    glGetIntegerv(GL_MINOR_VERSION, &actual_minor);
    if ( !(actual_major == 0 && actual_minor == 0)
         && (actual_major < gl_version_major
             || (actual_major == gl_version_major && actual_minor < gl_version_minor)) ) {
        milton_die_gracefully("This graphics driver does not support OpenGL 2.1+");
    }
    milton_log("Created OpenGL context with version %s\n", glGetString(GL_VERSION));
    milton_log("    and GLSL %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    // ==== Initialize milton

    MiltonState* milton_state = arena_bootstrap(MiltonState, root_arena, 1024*1024);

    if ( !gl::load() ) {
        milton_die_gracefully("Milton could not load the necessary OpenGL functionality. Exiting.");
    }

    // Ask for native events to poll tablet events.
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

    SDL_SysWMinfo sysinfo;
    SDL_VERSION(&sysinfo.version);

    // Platform-specific setup
#if defined(_MSC_VER)
#pragma warning (push, 0)
#endif
    if ( SDL_GetWindowWMInfo( window, &sysinfo ) ) {
        switch( sysinfo.subsystem ) {
#if defined(_WIN32)
            case SDL_SYSWM_WINDOWS: {
                { // Handle the case where the window was too big for the screen.
                    HWND hwnd = sysinfo.info.win.window;
                    if (!is_fullscreen) {
                        RECT res_rect;
                        RECT win_rect;
                        HWND dhwnd = GetDesktopWindow();
                        GetWindowRect(dhwnd, &res_rect);
                        GetClientRect(hwnd, &win_rect);

                        platform_state.hwnd = hwnd;

                        i32 snap_threshold = 300;
                        if (win_rect.right != platform_state.width
                            || win_rect.bottom != platform_state.height
                            // Also maximize if the size is large enough to "snap"
                            || (win_rect.right + snap_threshold >= res_rect.right
                                && win_rect.left + snap_threshold >= res_rect.left)
                            || win_rect.left < 0
                            || win_rect.top < 0) {
                            // Our prefs weren't right. Let's maximize.

                            SetWindowPos(hwnd, HWND_TOP, 20, 20, win_rect.right - 20, win_rect.bottom - 20, SWP_SHOWWINDOW);
                            platform_state.width = win_rect.right - 20;
                            platform_state.height = win_rect.bottom - 20;
                            ShowWindow(hwnd, SW_MAXIMIZE);
                        }
                    }
                }
                // Load EasyTab
                EasyTabResult easytab_res = EasyTab_Load(platform_state.hwnd);
                if (easytab_res != EASYTAB_OK) {
                    milton_log("EasyTab failed to load. Code %d\n", easytab_res);
                }
            } break;
#elif defined(__linux__)
            case SDL_SYSWM_X11: {
                EasyTab_Load(sysinfo.info.x11.display, sysinfo.info.x11.window);
            } break;
#endif
            default: {
            } break;
        }
    }
    else {
        milton_die_gracefully("Can't get system info!\n");
    }
#if defined(_MSC_VER)
#pragma warning (pop)
#endif

    platform_state.ui_scale = platform_ui_scale(&platform_state);
    milton_log("UI scale is %f\n", platform_state.ui_scale);
    // Initialize milton_state
    {
        milton_state->render_data = gpu_allocate_render_data(&milton_state->root_arena);

        PATH_CHAR* file_to_open_ = NULL;
        PATH_CHAR buffer[MAX_PATH] = {};

        if ( file_to_open ) {
            file_to_open_ = (PATH_CHAR*)buffer;
        }

        str_to_path_char(file_to_open, (PATH_CHAR*)file_to_open_, MAX_PATH*sizeof(*file_to_open_));

        milton_init(milton_state, platform_state.width, platform_state.height, platform_state.ui_scale, (PATH_CHAR*)file_to_open_);
        milton_state->gui->menu_visible = true;
        if ( is_fullscreen ) {
            milton_state->gui->menu_visible = false;
        }
    }
    milton_resize_and_pan(milton_state, {}, {platform_state.width, platform_state.height});

    platform_state.window_id = SDL_GetWindowID(window);

    // Init ImGUI
    //ImGui_ImplSdl_Init(window);
    ImGui_ImplSdlGL3_Init(window);

    i32 display_hz = platform_monitor_refresh_hz();

#if defined(_WIN32)
    {  // Load icon (Win32)
        int si = sizeof(HICON);
        HINSTANCE handle = GetModuleHandle(nullptr);
        PATH_CHAR icopath[MAX_PATH] = L"milton_icon.ico";
        platform_fname_at_exe(icopath, MAX_PATH);
        HICON icon = (HICON)LoadImageW(NULL, icopath, IMAGE_ICON, /*W*/0, /*H*/0,
                                       LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
        if ( icon != NULL ) {
            SendMessage(platform_state.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        }
    }

    // Setup hardware cursor.

#if MILTON_HARDWARE_BRUSH_CURSOR
    {  // Set brush HW cursor
        milton_log("Setting up hardware cursor.\n");
        size_t w = (size_t)GetSystemMetrics(SM_CXCURSOR);
        size_t h = (size_t)GetSystemMetrics(SM_CYCURSOR);

        size_t arr_sz = (w*h+7) / 8;

        char* andmask = arena_alloc_array(&milton_state->root_arena, arr_sz, char);
        char* xormask = arena_alloc_array(&milton_state->root_arena, arr_sz, char);

        i32 counter = 0;
        {
            size_t cx = w/2;
            size_t cy = h/2;
            for ( size_t j = 0; j < h; ++j ) {
                for ( size_t i = 0; i < w; ++i ) {
                    size_t dist = (i-cx)*(i-cx) + (j-cy)*(j-cy);

                    // 32x32 default;
                    i64 girth = 3; // girth of cursor in pixels
                    size_t radius = 8;
                    if ( w == 32 && h == 32 ) {
                        // 32x32
                    }
                    else if ( w == 64 && h == 64 ) {
                        girth *= 2;
                        radius *= 2;
                    }
                    else {
                        milton_log("WARNING: Got an unexpected cursor size of %dx%d. Using 32x32 and hoping for the best.\n", w, h);
                        w = 32;
                        h = 32;
                        cx = 16;
                        cy = 16;
                    }
                    i64 diff        = (i64)(dist - SQUARE(radius));
                    b32 in_white = diff < SQUARE(girth-0.5f) && diff > -SQUARE(girth-0.5f);
                    diff = (i64)(dist - SQUARE(radius+1));
                    b32 in_black = diff < SQUARE(girth) && diff > -SQUARE(girth);

                    size_t idx = j*w + i;

                    size_t ai = idx / 8;
                    size_t bi = idx % 8;

                    // This code block for windows CreateCursor
#if 0
                    if (incircle &&
                        // Cross-hair effect. Only pixels inside half-radius bands get drawn.
                        (i > cx-radius/2 && i < cx+radius/2 || j > cy-radius/2 && j < cy+radius/2))
                    {
                        if (toggle_black)
                        {
                            xormask[ai] |= (1 << (7 - bi));
                        }
                        else
                        {
                            xormask[ai] &= ~(1 << (7 - bi));
                            xormask[ai] &= ~(1 << (7 - bi));
                        }
                        toggle_black = !toggle_black;
                    }
                    else
                    {
                        andmask[ai] |= (1 << (7 - bi));
                    }
#endif
                    // SDL code block
                    if ( in_white ) {
                        // Cross-hair effect. Only pixels inside half-radius bands get drawn.
                        /* (i > cx-radius/2 && i < cx+radius/2 || j > cy-radius/2 && j < cy+radius/2)) */
                        andmask[ai] &= ~(1 << (7 - bi));  // White
                        xormask[ai] |= (1 << (7 - bi));
                    }
                    else if ( in_black ) {
                        xormask[ai] |= (1 << (7 - bi));  // Black
                        andmask[ai] |= (1 << (7 - bi));

                    }
                    else {
                        andmask[ai] &= ~(1 << (7 - bi));     // Transparent
                        xormask[ai] &= ~(1 << (7 - bi));
                    }
                }
            }
        }
        //platform_state.hcursor = CreateCursor(/*HINSTANCE*/ 0,
        //                                      /*xHotSpot*/(int)(w/2),
        //                                      /*yHotSpot*/(int)(h/2),
        //                                      /* nWidth */(int)w,
        //                                      /* nHeight */(int)h,
        //                                      (VOID*)andmask,
        //                                      (VOID*)xormask);

        platform_state.cursor_brush = SDL_CreateCursor((Uint8*)andmask,
                                                       (Uint8*)xormask,
                                                       (int)w,
                                                       (int)h,
                                                       /*xHotSpot*/(int)(w/2),
                                                       /*yHotSpot*/(int)(h/2));

    }

#endif // MILTON_HARDWARE_BRUSH_CURSOR
#endif // WIN32

    #if MILTON_HARDWARE_BRUSH_CURSOR
        mlt_assert(platform_state.cursor_brush != NULL);
    #endif

    // ImGui setup
    {
        milton_log("ImGUI setup\n");
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;  // Don't save any imgui.ini file
        PATH_CHAR fname[MAX_PATH] = TO_PATH_STR("Carlito.ttf");
        platform_fname_at_exe(fname, MAX_PATH);
        FILE* fd = platform_fopen(fname, TO_PATH_STR("rb"));

        if ( fd ) {
            size_t  ttf_sz = 0;
            void*   ttf_data = NULL;
            //ImFont* im_font =  io.Fonts->ImFontAtlas::AddFontFromFileTTF("carlito.ttf", 14);
            // Load file to memory
            if ( fseek(fd, 0, SEEK_END) == 0 ) {
                long ttf_sz_long = ftell(fd);
                if ( ttf_sz_long != -1 ) {
                    ttf_sz = (size_t)ttf_sz_long;
                    if ( fseek(fd, 0, SEEK_SET) == 0 ) {
                        ttf_data = ImGui::MemAlloc(ttf_sz);
                        if ( ttf_data ) {
                            if ( fread(ttf_data, 1, ttf_sz, fd) == ttf_sz ) {
                                ImFont* im_font = io.Fonts->ImFontAtlas::AddFontFromMemoryTTF(ttf_data, (int)ttf_sz, int(14*platform_state.ui_scale));
                            }
                            else {
                                milton_log("WARNING: Error reading TTF file");
                            }
                        }
                        else {
                            milton_log("WARNING: could not allocate data for font!");
                        }
                    }
                }
            }
            fclose(fd);
        }
    }
    // Initalize system cursors
    {
        platform_state.cursor_default   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        platform_state.cursor_hand      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
        platform_state.cursor_crosshair = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        platform_state.cursor_sizeall   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);

        cursor_set_and_show(platform_state.cursor_default);
    }

    // ---- Main loop ----

    while ( !platform_state.should_quit ) {
        PROFILE_GRAPH_END(system);
        PROFILE_GRAPH_BEGIN(polling);

        u64 frame_start_us = perf_counter();

        ImGuiIO& imgui_io = ImGui::GetIO();

        MiltonInput milton_input = sdl_event_loop(milton_state, &platform_state);

        // Handle pen orientation to switch to eraser or pen.
        if ( EasyTab != NULL && EasyTab->PenInProximity ) {
            static int previous_orientation = 0;

            // TODO: This logic needs to handle primitives, not just eraser/pen
            bool changed = false;
            if ( EasyTab->Orientation.Altitude < 0 && previous_orientation >= 0 ) {
                milton_input.mode_to_set = MiltonMode::ERASER;
                changed = true;
            }
            else if ( EasyTab->Orientation.Altitude > 0 && previous_orientation <= 0 ) {
                milton_input.mode_to_set = MiltonMode::PEN;
                changed = true;
            }
            if ( changed ) {
                previous_orientation = EasyTab->Orientation.Altitude;
            }
        }

        panning_update(&platform_state);

        if ( !platform_state.is_panning ) {
            milton_input.flags |= MiltonInputFlags_HOVERING;
            milton_input.hover_point = platform_state.pointer;
        }

        static b32 first_run = true;
        if ( first_run ) {
            first_run = false;
            milton_input.flags = MiltonInputFlags_FULL_REFRESH;
        }

        {
            int x = 0;
            int y = 0;
            SDL_GetMouseState(&x, &y);

            // Convert x,y to pixels
            {
               v2l v = { (long)x, (long)y };
               platform_point_to_pixel(&platform_state, &v);
               x = v.x;
               y = v.y;
            }

            // NOTE: Calling SDL_SetCursor more than once seems to cause flickering.

            // Handle system cursor and platform state related to current_mode
            {
                    static b32 was_exporting = false;

                    if ( platform_state.is_panning || platform_state.waiting_for_pan_input ) {
                        cursor_set_and_show(platform_state.cursor_sizeall);
                    }
                    // Show resize icon
                    #if !MILTON_HARDWARE_BRUSH_CURSOR
                        #define PAD 20
                        else if (x > milton_state->view->screen_size.w - PAD
                             || x < PAD
                             || y > milton_state->view->screen_size.h - PAD
                             || y < PAD ) {
                            cursor_set_and_show(platform_state.cursor_default);
                        }
                        #undef PAD
                    #endif
                    else if ( ImGui::GetIO().WantCaptureMouse ) {
                        cursor_set_and_show(platform_state.cursor_default);
                    }
                    else if ( milton_state->current_mode == MiltonMode::EXPORTING ) {
                        cursor_set_and_show(platform_state.cursor_crosshair);
                        was_exporting = true;
                    }
                    else if ( was_exporting ) {
                        cursor_set_and_show(platform_state.cursor_default);
                        was_exporting = false;
                    }
                    else if ( milton_state->current_mode == MiltonMode::EYEDROPPER ) {
                        cursor_set_and_show(platform_state.cursor_crosshair);
                        platform_state.is_pointer_down = false;
                    }
                    else if ( milton_state->gui->visible
                              && is_inside_rect_scalar(get_bounds_for_picker_and_colors(&milton_state->gui->picker), x,y) ) {
                        cursor_set_and_show(platform_state.cursor_default);
                    }
                    else if ( milton_state->current_mode == MiltonMode::PEN ||
                              milton_state->current_mode == MiltonMode::ERASER ||
                              milton_state->current_mode == MiltonMode::PRIMITIVE ) {
                        #if MILTON_HARDWARE_BRUSH_CURSOR
                            cursor_set_and_show(platform_state.cursor_brush);
                        #else
                            platform_cursor_hide();
                        #endif
                    }
                    else if ( milton_state->current_mode == MiltonMode::HISTORY ) {
                        cursor_set_and_show(platform_state.cursor_default);
                    }
                    else if ( milton_state->current_mode != MiltonMode::PEN || milton_state->current_mode != MiltonMode::ERASER ) {
                        platform_cursor_hide();
                    }
                }
        }
        // IN OSX: SDL polled all events, we get all the pressure inputs from our hook
#if defined(__MACH__)
        platform_state.num_pressure_results = 0;
        int num_polled_pressures = 0;

        float* polled_pressures = milton_osx_poll_pressures(&num_polled_pressures);
        if ( num_polled_pressures ) {
            for ( int i = num_polled_pressures - 1; i >= 0; --i ) {
                milton_input.pressures[platform_state.num_pressure_results++] = polled_pressures[i];
            }
        }
#endif

        i32 input_flags = (i32)milton_input.flags;

        ImGui_ImplSdlGL3_NewFrame(window);
        // Clear our pointer input because we captured an ImGui widget!
        if ( ImGui::GetIO().WantCaptureMouse ) {
            platform_state.num_point_results = 0;
            platform_state.is_pointer_down = false;
            input_flags |= MiltonInputFlags_IMGUI_GRABBED_INPUT;
        }

        milton_imgui_tick(&milton_input, &platform_state, milton_state);

        // Clear pan delta if we are zooming
        if ( milton_input.scale != 0 ) {
            milton_input.pan_delta = {};
            input_flags |= MiltonInputFlags_FULL_REFRESH;
        }
        else if ( platform_state.is_panning ) {
            input_flags |= MiltonInputFlags_PANNING;
            platform_state.num_point_results = 0;
        }
        else if ( platform_state.was_panning ) {
            // Just finished panning. Refresh the screen.
            input_flags |= MiltonInputFlags_FULL_REFRESH;
        }

        if ( platform_state.num_pressure_results < platform_state.num_point_results ) {
            platform_state.num_point_results = platform_state.num_pressure_results;
        }

        milton_input.flags = (MiltonInputFlags)( input_flags | (int)milton_input.flags );

        mlt_assert (platform_state.num_point_results <= platform_state.num_pressure_results);

        milton_input.input_count = platform_state.num_point_results;

        v2l pan_delta = platform_state.pan_point - platform_state.pan_start;
        if (    pan_delta.x != 0
             || pan_delta.y != 0
             || platform_state.width != milton_state->view->screen_size.x
             || platform_state.height != milton_state->view->screen_size.y ) {
            milton_resize_and_pan(milton_state, pan_delta, {platform_state.width, platform_state.height});
        }
        milton_input.pan_delta = pan_delta;

        // Reset pan_start. Delta is not cumulative.
        platform_state.pan_start = platform_state.pan_point;

        // ==== Update and render
        PROFILE_GRAPH_END(polling);
        PROFILE_GRAPH_BEGIN(GL);
        milton_update_and_render(milton_state, &milton_input);
        if ( !(milton_state->flags & MiltonStateFlags_RUNNING) ) {
            platform_state.should_quit = true;
        }
        ImGui::Render();
        PROFILE_GRAPH_END(GL);
        PROFILE_GRAPH_BEGIN(system);
        SDL_GL_SwapWindow(window);

#ifdef __linux__
        gtk_main_iteration_do(FALSE);
#endif
        // Sleep if the frame took less time than the refresh rate.
        u64 frame_time_us = perf_counter() - frame_start_us;

        f32 expected_us = (f32)1000000 / display_hz;
        if ( frame_time_us < expected_us ) {
            f32 to_sleep_us = expected_us - frame_time_us;
            //  milton_log("Sleeping at least %d ms\n", (u32)(to_sleep_us/1000));
            SDL_Delay((u32)(to_sleep_us/1000));
        }
        #if REDRAW_EVERY_FRAME
        platform_state.force_next_frame = true;
        #endif
        // IMGUI events might update until the frame after they are created.
        if ( !platform_state.force_next_frame ) {
            SDL_WaitEvent(NULL);
        }
    }

#if defined(_WIN32)
    EasyTab_Unload();
#endif

    arena_free(&milton_state->root_arena);

    if(!is_fullscreen) {
        bool save_prefs = prefs.width != platform_state.width || prefs.height != platform_state.height;
        if ( save_prefs ) {
            v2l size =  { platform_state.width,platform_state.height };
            platform_pixel_to_point(&platform_state, &size);

            prefs.width  = size.w;
            prefs.height = size.h;
            milton_prefs_save(&prefs);
        }
    }

    SDL_Quit();

    return 0;
}
