// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM "gl.h"
#include <imgui.h>
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "milton.h"
#include "gl_helpers.h"
#include "gui.h"
#include "persist.h"
#include "bindings.h"


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
shortcut_handle_key(Milton* milton, PlatformState* platform, SDL_Event* event, MiltonInput* input, b32 is_keyup)
{
    ImGuiIO& io = ImGui::GetIO();

    if (io.WantCaptureKeyboard) {
        int key = event->key.keysym.scancode;
        IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
        io.KeysDown[key] = (event->type == SDL_KEYDOWN);
        io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
        io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
        io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
        io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
    }
    else {
        MiltonBindings* bindings = &milton->settings->bindings;

        SDL_Keymod m = SDL_GetModState();
        SDL_Keycode k = event->key.keysym.sym;

        i8 active_key = 0;
        if (k >= 1 && k <= 127) {
            active_key = k;
        }
        else {
            switch (k) {
                case SDLK_F1:  { active_key = Binding::F1;  } break;
                case SDLK_F2:  { active_key = Binding::F2;  } break;
                case SDLK_F3:  { active_key = Binding::F3;  } break;
                case SDLK_F4:  { active_key = Binding::F4;  } break;
                case SDLK_F5:  { active_key = Binding::F5;  } break;
                case SDLK_F6:  { active_key = Binding::F6;  } break;
                case SDLK_F7:  { active_key = Binding::F7;  } break;
                case SDLK_F8:  { active_key = Binding::F8;  } break;
                case SDLK_F9:  { active_key = Binding::F9;  } break;
                case SDLK_F10: { active_key = Binding::F10; } break;
                case SDLK_F11: { active_key = Binding::F11; } break;
                case SDLK_F12: { active_key = Binding::F12; } break;
                default: {  } break;
            }
        }

        u32 active_modifiers = 0;

        if (m & KMOD_CTRL) { active_modifiers |= Modifier_CTRL; }
        if (m & KMOD_SHIFT) { active_modifiers |= Modifier_SHIFT; }
        if (m & KMOD_GUI) { active_modifiers |= Modifier_WIN; }
        if (m & KMOD_ALT) { active_modifiers |= Modifier_ALT; }
        if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_SPACE]) { active_modifiers |= Modifier_SPACE; }

        if (is_keyup) {

            // Switch on k again, this time catching when some of the modifiers were released.
            switch (k) {
                case SDLK_LSHIFT:
                case SDLK_RSHIFT: {
                    active_modifiers |= Modifier_SHIFT;
                } break;
                case SDLK_LALT:
                case SDLK_RALT: {
                    active_modifiers |= Modifier_ALT;
                } break;
                case SDLK_LGUI:
                case SDLK_RGUI: {
                    active_modifiers |= Modifier_WIN;
                } break;
                case SDLK_LCTRL:
                case SDLK_RCTRL: {
                    active_modifiers |= Modifier_CTRL;
                } break;
            }

            for (sz i = Action_COUNT + 1; i < Action_COUNT_WITH_RELEASE; ++i) {
                Binding* b = &bindings->bindings[i];
                if ( (!event->key.repeat || b->accepts_repeats) &&
                     active_modifiers == b->modifiers &&
                     active_key == b->bound_key &&
                     b->on_release &&
                     b->action != Action_NONE ) {
                    binding_dispatch_action(b->action, input, milton, platform->pointer);
                    platform->force_next_frame = true;
                }
            }
        }
        // keydown
        else  {
            for (sz i = 0; i < Action_COUNT; ++i) {
                Binding* b = &bindings->bindings[i];

                if ( (!event->key.repeat || b->accepts_repeats) &&
                     active_modifiers == b->modifiers &&
                     active_key == b->bound_key &&
                     !b->on_release &&
                     b->action != Action_NONE ) {
                    binding_dispatch_action(b->action, input, milton, platform->pointer);
                    platform->force_next_frame = true;
                }
            }

        }
        if ( k == SDLK_SPACE && !is_keyup ) {
            platform->is_space_down = true;
        }
    }
}

void
panning_update(PlatformState* platform)
{
    auto reset_pan_start = [platform]() {
        platform->pan_start = VEC2L(platform->pointer);
        platform->pan_point = platform->pan_start;  // No huge pan_delta at beginning of pan.
    };

    platform->was_panning = platform->is_panning;

    // Panning from GUI menu, waiting for input
    if ( platform->waiting_for_pan_input ) {
        if ( platform->is_pointer_down ) {
            platform->waiting_for_pan_input = false;
            platform->is_panning = true;
            reset_pan_start();
        }
        // Space cancels waiting
        if ( platform->is_space_down ) {
            platform->waiting_for_pan_input = false;
        }
    }
    else {
        if ( platform->is_panning ) {
            if ( (!platform->is_pointer_down && !platform->is_space_down)
                 || !platform->is_pointer_down ) {
                platform->is_panning = false;
            }
            else {
                platform->pan_point = VEC2L(platform->pointer);
            }
        }
        else {
            if ( (platform->is_space_down && platform->is_pointer_down)
                 || platform->is_middle_button_down ) {
                platform->is_panning = true;
                reset_pan_start();
            }
        }
    }
}

MiltonInput
sdl_event_loop(Milton* milton, PlatformState* platform)
{
    MiltonInput milton_input = {};
    milton_input.mode_to_set = MiltonMode::MODE_COUNT;

    b32 pointer_up = false;

    v2i input_point = {};

    platform->num_pressure_results = 0;
    platform->num_point_results = 0;
    platform->keyboard_layout = get_current_keyboard_layout();

    SDL_Event event;
    while ( SDL_PollEvent(&event) ) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        SDL_Keymod keymod = SDL_GetModState();

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
            case SDL_QUIT: {
                platform_cursor_show();
                milton_try_quit(milton);
            } break;
            case SDL_SYSWMEVENT: {
                f32 pressure = NO_PRESSURE_INFO;
                SDL_SysWMEvent sysevent = event.syswm;
                EasyTabResult er = EASYTAB_EVENT_NOT_HANDLED;
                if (!EasyTab) { break; }

                i32 bit_touch_old = (EasyTab->Buttons & EasyTab_Buttons_Pen_Touch);

                er = platform_handle_sysevent(platform, &sysevent);

                if ( er == EASYTAB_OK ) {
                    i32 bit_touch = (EasyTab->Buttons & EasyTab_Buttons_Pen_Touch);
                    i32 bit_lower = (EasyTab->Buttons & EasyTab_Buttons_Pen_Lower);
                    i32 bit_upper = (EasyTab->Buttons & EasyTab_Buttons_Pen_Upper);

                    // Pen in use but not drawing
                    b32 taking_pen_input = EasyTab->PenInProximity
                                           && bit_touch
                                           && !( bit_upper || bit_lower );

                    if ( taking_pen_input ) {
                        platform->is_pointer_down = true;

                        for ( int pi = 0; pi < EasyTab->NumPackets; ++pi ) {
                            v2l point = { EasyTab->PosX[pi], EasyTab->PosY[pi] };

                            platform_point_to_pixel(platform, &point);

                            if ( point.x >= 0 && point.y >= 0 ) {
                                if ( platform->num_point_results < MAX_INPUT_BUFFER_ELEMS ) {
                                    milton_input.points[platform->num_point_results++] = point;
                                }
                                if ( platform->num_pressure_results < MAX_INPUT_BUFFER_ELEMS ) {
                                    milton_input.pressures[platform->num_pressure_results++] = EasyTab->Pressure[pi];
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

                        platform_point_to_pixel_i(platform, &point);

                        platform->pointer = point;
                    }
                }

                if (er == EASYTAB_NEEDS_REINIT) {
                    platform_dialog("Tablet information changed. You might want to restart Milton", "Tablet info changed.");
                }
            } break;
            case SDL_MOUSEBUTTONDOWN: {
                if ( event.button.windowID != platform->window_id ) {
                    break;
                }

                if (   (event.button.button == SDL_BUTTON_LEFT && ( EasyTab == NULL || !EasyTab->PenInProximity))
                     || event.button.button == SDL_BUTTON_MIDDLE
                     // Ignoring right click events for now
                     /*|| event.button.button == SDL_BUTTON_RIGHT*/ ) {

                    if ( ImGui::GetIO().WantCaptureMouse ) {
                        platform->force_next_frame = true;
                    }
                    else {
                        v2l long_point = { event.button.x, event.button.y };

                        platform_point_to_pixel(platform, &long_point);

                        v2i point = v2i{(int)long_point.x, (int)long_point.y};

                        if ( !platform->is_panning && point.x >= 0 && point.y > 0 ) {
                            milton_input.click = point;

                            platform->is_pointer_down = true;
                            platform->pointer = point;
                            platform->is_middle_button_down = (event.button.button == SDL_BUTTON_MIDDLE);

                            if ( platform->num_point_results < MAX_INPUT_BUFFER_ELEMS ) {
                                milton_input.points[platform->num_point_results++] = VEC2L(point);
                            }
                            if ( platform->num_pressure_results < MAX_INPUT_BUFFER_ELEMS ) {
                                milton_input.pressures[platform->num_pressure_results++] = NO_PRESSURE_INFO;
                            }
                        }
                    }
                }
            } break;
            case SDL_MOUSEBUTTONUP: {
                if ( event.button.windowID != platform->window_id ) {
                    break;
                }
                if ( event.button.button == SDL_BUTTON_LEFT
                     || event.button.button == SDL_BUTTON_MIDDLE
                     || event.button.button == SDL_BUTTON_RIGHT ) {
                    if ( event.button.button == SDL_BUTTON_MIDDLE ) {
                        platform->is_middle_button_down = false;
                    }
                    if ( ImGui::GetIO().WantCaptureMouse ) {
                        // NOTE(ameen): button-click events that cause UI changes have 1 frame delay to update.
                        platform->force_next_frame = true;
                    }
                    pointer_up = true;
                    milton_input.flags |= MiltonInputFlags_CLICKUP;
                    milton_input.flags |= MiltonInputFlags_END_STROKE;
                }
            } break;
            case SDL_MOUSEMOTION: {
                if (event.motion.windowID != platform->window_id) {
                    break;
                }

                input_point = {event.motion.x, event.motion.y};

                platform_point_to_pixel_i(platform, &input_point);

                platform->pointer = input_point;

                // In case the wacom driver craps out, or anything goes wrong (like the event queue
                // overflowing ;)) then we default to receiving WM_MOUSEMOVE. If we catch a single
                // point, then it's fine. It will get filtered out in milton_stroke_input

                if (EasyTab == NULL || !EasyTab->PenInProximity) {
                    if (platform->is_pointer_down) {
                        if (!platform->is_panning &&
                            (input_point.x >= 0 && input_point.y >= 0)) {
                            if (platform->num_point_results < MAX_INPUT_BUFFER_ELEMS) {
                                milton_input.points[platform->num_point_results++] = VEC2L(input_point);
                            }
                            if (platform->num_pressure_results < MAX_INPUT_BUFFER_ELEMS) {
                                milton_input.pressures[platform->num_pressure_results++] = NO_PRESSURE_INFO;
                            }
                        }
                    }
                }
                break;
            }
            case SDL_MOUSEWHEEL: {
                if ( event.wheel.windowID != platform->window_id ) {
                    break;
                }
                if ( !ImGui::GetIO().WantCaptureMouse ) {
                    milton_input.scale += event.wheel.y;
                    v2i zoom_center = platform->pointer;

                    milton_set_zoom_at_point(milton, zoom_center);
                    // ImGui has a delay of 1 frame when displaying zoom info.
                    // Force next frame to have the value up to date.
                    platform->force_next_frame = true;
                }

                break;
            }
            case SDL_KEYDOWN: {
                shortcut_handle_key(milton, platform, &event, &milton_input, /*is_keyup*/false);
            } break;
            case SDL_KEYUP: {
                if ( event.key.windowID != platform->window_id ) {
                    break;
                }

                SDL_Keycode keycode = event.key.keysym.sym;

                if ( keycode == SDLK_SPACE ) {
                    platform->is_space_down = false;
                }
                shortcut_handle_key(milton, platform, &event, &milton_input, /*is_keyup*/true);
            } break;
            case SDL_WINDOWEVENT: {
                if ( platform->window_id != event.window.windowID ) {
                    break;
                }
                switch ( event.window.event ) {
                    // Just handle every event that changes the window size.
                case SDL_WINDOWEVENT_MOVED:
                    platform->num_point_results = 0;
                    platform->num_pressure_results = 0;
                    platform->is_pointer_down = false;
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED: {

                    v2i size = { event.window.data1, event.window.data2 };
                    platform_point_to_pixel_i(platform, &size);

                    platform->width = size.w;
                    platform->height = size.h;


                    milton_input.flags |= MiltonInputFlags_FULL_REFRESH;
                    glViewport(0, 0, platform->width, platform->height);
                    break;
                }
                case SDL_WINDOWEVENT_LEAVE:
                    if ( event.window.windowID != platform->window_id ) {
                        break;
                    }
                    if ( milton->current_mode != MiltonMode::DRAG_BRUSH_SIZE ) {
                        platform_cursor_show();
                    }
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
        if ( platform->should_quit ) {
            break;
        }
    }  // ---- End of SDL event loop

    if ( pointer_up ) {
        // Add final point
        if ( !platform->is_panning && platform->is_pointer_down ) {
            milton_input.flags |= MiltonInputFlags_END_STROKE;
            input_point = { event.button.x, event.button.y };

            platform_point_to_pixel_i(platform, &input_point);

            if ( platform->num_point_results < MAX_INPUT_BUFFER_ELEMS ) {
                milton_input.points[platform->num_point_results++] = VEC2L(input_point);
            }
        }
        platform->is_pointer_down = false;

        platform->num_point_results = 0;
    }

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

    PlatformState platform = {};

    PlatformSettings prefs = {};

    milton_log("Loading preferences...\n");
    if ( platform_settings_load(&prefs) ) {
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

    platform.ui_scale = 1.0f;

    platform.keyboard_layout = get_current_keyboard_layout();

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

    platform.window = window;

    // Milton works in pixels, but macOS works distinguishing "points" and
    // "pixels", with most APIs working in points.

    v2l size_px = { window_width, window_height };
    platform_point_to_pixel(&platform, &size_px);

    platform.width = size_px.w;
    platform.height = size_px.h;

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if ( !gl_context ) {
        milton_die_gracefully("Could not create OpenGL context\n");
    }

    if ( !gl::load() ) {
        milton_die_gracefully("Milton could not load the necessary OpenGL functionality. Exiting.");
    }

    // Init ImGUI
    ImGui::CreateContext();


#if USE_GL_3_2
    const char* gl_version = "#version 330 \n";
#else
    const char* gl_version = "#version 120 \n";
#endif

    ImGui_ImplSDL2_InitForOpenGL(window, &gl_context);
    ImGui_ImplOpenGL3_Init(gl_version);

    SDL_GL_SetSwapInterval(1);

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

    Milton* milton = arena_bootstrap(Milton, root_arena, 1024*1024);

    // Ask for native events to poll tablet events.
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

    SDL_SysWMinfo sysinfo;
    SDL_VERSION(&sysinfo.version);

    // Platform-specific setup
#if defined(_MSC_VER)
#pragma warning (push, 0)
#endif
    if ( SDL_GetWindowWMInfo( window, &sysinfo ) ) {
        platform_init(&platform, &sysinfo);
    }
    else {
        milton_die_gracefully("Can't get system info!\n");
    }
#if defined(_MSC_VER)
#pragma warning (pop)
#endif

    platform.ui_scale = platform_ui_scale(&platform);
    milton_log("UI scale is %f\n", platform.ui_scale);
    // Initialize milton
    PATH_CHAR* file_to_open_ = NULL;
    PATH_CHAR buffer[MAX_PATH] = {};

    if ( file_to_open ) {
        file_to_open_ = (PATH_CHAR*)buffer;
    }

    str_to_path_char(file_to_open, (PATH_CHAR*)file_to_open_, MAX_PATH*sizeof(*file_to_open_));

    milton_init(milton, platform.width, platform.height, platform.ui_scale, (PATH_CHAR*)file_to_open_);
    milton->platform = &platform;
    milton->gui->menu_visible = true;
    if ( is_fullscreen ) {
        milton->gui->menu_visible = false;
    }

    milton_resize_and_pan(milton, {}, {platform.width, platform.height});

    platform.window_id = SDL_GetWindowID(window);

    i32 display_hz = platform_monitor_refresh_hz();

    platform_setup_cursor(&milton->root_arena, &platform);

    // Sometimes SDL sets the window position such that it's impossible to move
    // without using Windows shortcuts that not everyone knows. Check if this
    // is the case and set a good default.
    if (!is_fullscreen) {
        const int pixel_padding = platform_titlebar_height(&platform);
        int x = 0, y = 0;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_SetWindowPosition(window, min(max(0, x), platform.width - pixel_padding), min(max(pixel_padding, y), platform.height  - pixel_padding));
    }

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
                                ImFont* im_font = io.Fonts->ImFontAtlas::AddFontFromMemoryTTF(ttf_data, (int)ttf_sz, int(14*platform.ui_scale));
                            }
                            else {
                                milton_log("WARNING: Error reading TTF file\n");
                            }
                        }
                        else {
                            milton_log("WARNING: could not allocate data for font!\n");
                        }
                    }
                }
            }
            fclose(fd);
        }
    }
    // Initalize system cursors
    {
        platform.cursor_default   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        platform.cursor_hand      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
        platform.cursor_crosshair = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        platform.cursor_sizeall   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);

        cursor_set_and_show(platform.cursor_default);
    }

    // ---- Main loop ----

    while ( !platform.should_quit ) {
        PROFILE_GRAPH_END(system);
        PROFILE_GRAPH_BEGIN(polling);

        u64 frame_start_us = perf_counter();

        ImGuiIO& imgui_io = ImGui::GetIO();

        MiltonInput milton_input = sdl_event_loop(milton, &platform);

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

        panning_update(&platform);

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
                platform_point_to_pixel(&platform, &v);
                x = v.x;
                y = v.y;
            }

            // NOTE: Calling SDL_SetCursor more than once seems to cause flickering.

            // Handle system cursor and platform state related to current_mode
            {
                    static b32 was_exporting = false;

                    if ( platform.is_panning || platform.waiting_for_pan_input ) {
                        cursor_set_and_show(platform.cursor_sizeall);
                    }
                    // Show resize icon
                    #if !MILTON_HARDWARE_BRUSH_CURSOR
                        #define PAD 20
                        else if (x > milton->view->screen_size.w - PAD
                             || x < PAD
                             || y > milton->view->screen_size.h - PAD
                             || y < PAD ) {
                            cursor_set_and_show(platform.cursor_default);
                        }
                        #undef PAD
                    #endif
                    else if ( ImGui::GetIO().WantCaptureMouse ) {
                        cursor_set_and_show(platform.cursor_default);
                    }
                    else if ( milton->current_mode == MiltonMode::EXPORTING ) {
                        cursor_set_and_show(platform.cursor_crosshair);
                        was_exporting = true;
                    }
                    else if ( was_exporting ) {
                        cursor_set_and_show(platform.cursor_default);
                        was_exporting = false;
                    }
                    else if ( milton->current_mode == MiltonMode::EYEDROPPER ) {
                        cursor_set_and_show(platform.cursor_crosshair);
                        platform.is_pointer_down = false;
                    }
                    else if ( milton->gui->visible
                              && is_inside_rect_scalar(get_bounds_for_picker_and_colors(&milton->gui->picker), x,y) ) {
                        cursor_set_and_show(platform.cursor_default);
                    }
                    else if ( milton->current_mode == MiltonMode::PEN ||
                              milton->current_mode == MiltonMode::ERASER ||
                              milton->current_mode == MiltonMode::PRIMITIVE ) {
                        #if MILTON_HARDWARE_BRUSH_CURSOR
                            cursor_set_and_show(platform.cursor_brush);
                        #else
                            platform_cursor_hide();
                        #endif
                    }
                    else if ( milton->current_mode == MiltonMode::HISTORY ) {
                        cursor_set_and_show(platform.cursor_default);
                    }
                    else if ( milton->current_mode == MiltonMode::DRAG_BRUSH_SIZE ) {
                        platform_cursor_hide();
                    }
                    else if ( milton->current_mode != MiltonMode::PEN || milton->current_mode != MiltonMode::ERASER ) {
                        platform_cursor_hide();
                    }
                }
        }
        // NOTE:
        //  Previous Milton versions had a hack where SDL was modified to call
        //  milton_osx_tablet_hook, where it would fill up some arrays.
        //  Here we would call milton_osx_poll_pressures to access those arrays.
        //
        //  OSX support is currently in limbo. Those two functions still exist
        //  but are not called anywhere.
        //    -Sergio 2018/07/08

        i32 input_flags = (i32)milton_input.flags;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        // Avoid the case where we stop changing the brush size when we hover over GUI elements.
        if ( milton->current_mode == MiltonMode::DRAG_BRUSH_SIZE ) {
            ImGui::GetIO().WantCaptureMouse = false;
        }

        // Clear our pointer input because we captured an ImGui widget!
        if ( ImGui::GetIO().WantCaptureMouse ) {
            platform.num_point_results = 0;
            platform.is_pointer_down = false;
            input_flags |= MiltonInputFlags_IMGUI_GRABBED_INPUT;
        }

        milton_imgui_tick(&milton_input, &platform, milton, &prefs);

        // Clear pan delta if we are zooming
        if ( milton_input.scale != 0 ) {
            milton_input.pan_delta = {};
        }
        else if ( platform.is_panning ) {
            input_flags |= MiltonInputFlags_PANNING;
            platform.num_point_results = 0;
        }
        else if ( platform.was_panning ) {
            // Just finished panning. Refresh the screen.
            input_flags |= MiltonInputFlags_FULL_REFRESH;
        }

        if ( platform.num_pressure_results < platform.num_point_results ) {
            platform.num_point_results = platform.num_pressure_results;
        }

        milton_input.flags = (MiltonInputFlags)( input_flags | (int)milton_input.flags );

        mlt_assert (platform.num_point_results <= platform.num_pressure_results);

        milton_input.input_count = platform.num_point_results;

        v2l pan_delta = platform.pan_point - platform.pan_start;
        if (    pan_delta.x != 0
             || pan_delta.y != 0
             || platform.width != milton->view->screen_size.x
             || platform.height != milton->view->screen_size.y ) {
            milton_resize_and_pan(milton, pan_delta, {platform.width, platform.height});
        }
        milton_input.pan_delta = pan_delta;

        // Reset pan_start. Delta is not cumulative.
        platform.pan_start = platform.pan_point;

        // ==== Update and render
        PROFILE_GRAPH_END(polling);
        PROFILE_GRAPH_BEGIN(GL);
        milton_update_and_render(milton, &milton_input);
        if ( !(milton->flags & MiltonStateFlags_RUNNING) ) {
            platform.should_quit = true;
        }
        {
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            ImGui::Render();
            SDL_GL_MakeCurrent(window, gl_context);
            PUSH_GRAPHICS_GROUP("ImGui");
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            POP_GRAPHICS_GROUP();
        }
        PROFILE_GRAPH_END(GL);
        PROFILE_GRAPH_BEGIN(system);
        SDL_GL_SwapWindow(window);

        platform_event_tick();

        // Sleep if the frame took less time than the refresh rate.
        u64 frame_time_us = perf_counter() - frame_start_us;

        f32 expected_us = (f32)1000000 / display_hz;
        if ( frame_time_us < expected_us ) {
            f32 to_sleep_us = expected_us - frame_time_us;
            //  milton_log("Sleeping at least %d ms\n", (u32)(to_sleep_us/1000));
            SDL_Delay((u32)(to_sleep_us/1000));
        }
        #if REDRAW_EVERY_FRAME
        platform.force_next_frame = true;
        #endif
        // IMGUI events might update until the frame after they are created.
        if ( !platform.force_next_frame ) {
            SDL_WaitEvent(NULL);
        }
        else {
            platform.force_next_frame = false;
        }
    }

    platform_deinit(&platform);

    arena_free(&milton->root_arena);

    // Save preferences.
    v2l size =  { platform.width,platform.height };
    platform_pixel_to_point(&platform, &size);

    prefs.width  = size.w;
    prefs.height = size.h;
    platform_settings_save(&prefs);

    SDL_Quit();

    return 0;
}
