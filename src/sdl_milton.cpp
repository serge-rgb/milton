// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "common.h"

#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include "gui.h"
#include "history_debugger.h"
#include "milton.h"
#include "persist.h"
#include "platform.h"
#include "platform_prefs.h"
#include "profiler.h"
#include "utils.h"

enum PanningFSM
{
    PanningFSM_NOTHING,
    PanningFSM_MOUSE_PANNING,
};

static b32 g_cursor_count = 0;

void cursor_hide()
{
#if defined(_WIN32)
    while (g_cursor_count >= 0)
    {
        ShowCursor(FALSE);
        g_cursor_count--;
    }
#else
    // TODO: haven't checked if this works.
    int lvl = SDL_ShowCursor(-1);
    if ( lvl >= 0 )
    {
        assert ( lvl == 1 );
        int res = SDL_ShowCursor(0);
        if (res < 0)
        {
            assert(!"wtf");
        }
    }
#endif
}

void cursor_show()
{
#if defined(_WIN32)
    while (g_cursor_count < 0)
    {
        ShowCursor(TRUE);
        g_cursor_count++;
    }
#else
    int lvl = SDL_ShowCursor(-1);
    if ( lvl < 0 )
    {
        assert ( lvl == -1 );
        SDL_ShowCursor(1);
    }
#endif
}

static void turn_panning_on(PlatformState* p)
{
    p->is_space_down = true;
    p->is_panning = true;
}

static void turn_panning_off(PlatformState* p)
{
    if (p->is_panning)
    {
        if (p->is_pointer_down)
        {
            p->panning_fsm = PanningFSM_MOUSE_PANNING;
        }
        else
        {
            p->is_panning = false;
            p->stopped_panning = true;
        }
    }
}

static void cursor_set_and_show(SDL_Cursor* cursor)
{
    SDL_SetCursor(cursor);
    cursor_show();
}

MiltonInput sdl_event_loop(MiltonState* milton_state, PlatformState* platform_state)
{
    MiltonInput milton_input = {};

    b32 pointer_up = false;

    v2i input_point = {};

    platform_state->num_pressure_results = 0;
    platform_state->num_point_results = 0;
    platform_state->stopped_panning = false;

    i32 input_flags = (i32)MiltonInputFlags_NONE;

    SDL_Event event;
    while ( SDL_PollEvent(&event) )
    {
        //ImGui_ImplSdl_ProcessEvent(&event);
        ImGui_ImplSdlGL3_ProcessEvent(&event);

        SDL_Keymod keymod = SDL_GetModState();
        platform_state->is_ctrl_down = (keymod & KMOD_LCTRL) | (keymod & KMOD_RCTRL);
        platform_state->is_shift_down = (keymod & KMOD_SHIFT);

#if 0
        if ( (keymod & KMOD_ALT) )
        {
            set_flag(input_flags, MiltonInputFlags_CHANGE_MODE);
            milton_input.mode_to_set = MiltonMode_EYEDROPPER;
        }
#endif


#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4061)
#endif
        switch ( event.type )
        {
        case SDL_QUIT:
            cursor_show();
            milton_try_quit(milton_state);
            break;
        case SDL_SYSWMEVENT:
            {
                f32 pressure = NO_PRESSURE_INFO;
                v2i point = { 0 };
                SDL_SysWMEvent sysevent = event.syswm;
                EasyTabResult er = EASYTAB_EVENT_NOT_HANDLED;
                switch(sysevent.msg->subsystem)
                {
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
                if ( er == EASYTAB_OK )
                {
                    if (platform_state->panning_fsm != PanningFSM_MOUSE_PANNING && EasyTab->Pressure > 0)
                    {
                        platform_state->is_pointer_down = true;
                        if (EasyTab->PosX >= 0 && EasyTab->PosY >= 0)  // Quick n' dirty is-inside-client-rect test
                        {
                            if ( platform_state->num_point_results < MAX_INPUT_BUFFER_ELEMS )
                            {
                                milton_input.points[platform_state->num_point_results++] = { EasyTab->PosX, EasyTab->PosY };
                            }
                            if ( platform_state->num_pressure_results < MAX_INPUT_BUFFER_ELEMS )
                            {
                                milton_input.pressures[platform_state->num_pressure_results++] = EasyTab->Pressure;
                            }
                        }
                    }
                }
            } break;
        case SDL_MOUSEBUTTONDOWN:
            if ( event.button.windowID != platform_state->window_id )
            {
                break;
            }
            if ( event.button.button == SDL_BUTTON_LEFT )
            {
                if ( !ImGui::GetIO().WantCaptureMouse )
                {
                    set_flag(input_flags, MiltonInputFlags_CLICK);
                    milton_input.click = { event.button.x, event.button.y };
                }

                if ( platform_state->num_pressure_results < MAX_INPUT_BUFFER_ELEMS )
                {
                    milton_input.points[platform_state->num_point_results++] = { event.button.x, event.button.y };
                }
                platform_state->is_pointer_down = true;
                if ( platform_state->is_panning )
                {
                    platform_state->pan_start = { event.button.x, event.button.y };
                    platform_state->pan_point = platform_state->pan_start;  // No huge pan_delta at beginning of pan.
                }
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if ( event.button.windowID != platform_state->window_id )
            {
                break;
            }
            if ( event.button.button == SDL_BUTTON_LEFT )
            {
                pointer_up = true;
                set_flag(input_flags, MiltonInputFlags_CLICKUP);
            }
            break;
        case SDL_MOUSEMOTION:
            {
                if ( event.motion.windowID != platform_state->window_id )
                {
                    break;
                }
                input_point = { event.motion.x, event.motion.y };
                if ( platform_state->is_pointer_down )
                {
                    if ( !platform_state->is_panning )
                    {
                        milton_input.points[platform_state->num_point_results++] = input_point;
                    }
                    else if ( platform_state->is_panning )
                    {
                        platform_state->pan_point = input_point;
                    }
                    unset_flag(input_flags, MiltonInputFlags_HOVERING);
                }
                else
                {
                    set_flag(input_flags, MiltonInputFlags_HOVERING);
                    milton_input.hover_point = input_point;
                }
                break;
            }
        case SDL_MOUSEWHEEL:
            {
                if ( event.wheel.windowID != platform_state->window_id )
                {
                    break;
                }
                if ( !ImGui::GetIO().WantCaptureMouse )
                {
                    milton_input.scale += event.wheel.y;
                }

                break;
            }
        case SDL_KEYDOWN:
            {
                if ( event.wheel.windowID != platform_state->window_id )
                {
                    break;
                }

                SDL_Keycode keycode = event.key.keysym.sym;

                // Actions accepting key repeats.
                {
                    if (keycode == SDLK_LEFTBRACKET)
                    {
                        milton_decrease_brush_size(milton_state);
                    }
                    else if (keycode == SDLK_RIGHTBRACKET)
                    {
                        milton_increase_brush_size(milton_state);
                    }
                    if ( platform_state->is_ctrl_down )
                    {
                        if (keycode == SDLK_z)
                        {
                            if (platform_state->is_shift_down)
                            {
                                set_flag(input_flags, MiltonInputFlags_REDO);
                            }
                            else
                            {
                                set_flag(input_flags, MiltonInputFlags_UNDO);
                            }
                        }
                        else if (keycode == SDLK_EQUALS)
                        {
                            milton_input.scale++;
                        }
                        else if (keycode == SDLK_MINUS)
                        {
                            milton_input.scale--;
                        }
                    }

                }
                if (event.key.repeat)
                {
                    break;
                }

                // Stop stroking when any key is hit
                platform_state->is_pointer_down = false;
                set_flag(input_flags, MiltonInputFlags_END_STROKE);

                if (keycode == SDLK_SPACE)
                {
                    turn_panning_on(platform_state);
                    // Stahp
                }
                if (platform_state->is_ctrl_down) {  // Ctrl-KEY with no key repeats.
                    if (keycode == SDLK_e)
                    {
                        set_flag(input_flags, MiltonInputFlags_CHANGE_MODE);
                        milton_input.mode_to_set = MiltonMode_EXPORTING;
                    }
                    if (keycode == SDLK_q)
                    {
                        milton_try_quit(milton_state);
                    }
                }
                else
                {
                    if (!ImGui::GetIO().WantCaptureMouse )
                    {
                        if (keycode == SDLK_e)
                        {
                            set_flag(input_flags, MiltonInputFlags_CHANGE_MODE);
                            milton_input.mode_to_set = MiltonMode_ERASER;
                        }
                        else if (keycode == SDLK_b)
                        {
                            set_flag(input_flags, MiltonInputFlags_CHANGE_MODE);
                            milton_input.mode_to_set = MiltonMode_PEN;
                        }
                        else if (keycode == SDLK_i)
                        {
                            set_flag(input_flags, MiltonInputFlags_CHANGE_MODE);
                            milton_input.mode_to_set = MiltonMode_EYEDROPPER;
                        }
                        else if (keycode == SDLK_TAB)
                        {
                            gui_toggle_visibility(milton_state->gui);
                        }
                        else if (keycode == SDLK_F1)
                        {
                            gui_toggle_help(milton_state->gui);
                        }
                        else if (keycode == SDLK_1)
                        {
                            milton_set_pen_alpha(milton_state, 0.1f);
                        }
                        else if (keycode == SDLK_2)
                        {
                            milton_set_pen_alpha(milton_state, 0.2f);
                        }
                        else if (keycode == SDLK_3)
                        {
                            milton_set_pen_alpha(milton_state, 0.3f);
                        }
                        else if ( keycode == SDLK_4 )
                        {
                            milton_set_pen_alpha(milton_state, 0.4f);
                        }
                        else if (keycode == SDLK_5)
                        {
                            milton_set_pen_alpha(milton_state, 0.5f);
                        }
                        else if (keycode == SDLK_6)
                        {
                            milton_set_pen_alpha(milton_state, 0.6f);
                        }
                        else if (keycode == SDLK_7)
                        {
                            milton_set_pen_alpha(milton_state, 0.7f);
                        }
                        else if (keycode == SDLK_8)
                        {
                            milton_set_pen_alpha(milton_state, 0.8f);
                        }
                        else if (keycode == SDLK_9)
                        {
                            milton_set_pen_alpha(milton_state, 0.9f);
                        }
                        else if (keycode == SDLK_0)
                        {
                            milton_set_pen_alpha(milton_state, 1.0f);
                        }
                    }
#if MILTON_DEBUG
                    if (keycode == SDLK_F4)
                    {
                        milton_log("[DEBUG]: Switching to %s renderer.\n",
                                   milton_state->DEBUG_sse2_switch ? "SSE" : "slow");
                        profiler_reset();
                        milton_state->DEBUG_sse2_switch = !milton_state->DEBUG_sse2_switch;
                    }
#endif
                }

                break;
            }
        case SDL_KEYUP:
            {
                if (event.key.windowID != platform_state->window_id) {
                break;
                }

                SDL_Keycode keycode = event.key.keysym.sym;

                if (keycode == SDLK_SPACE)
                {
                    turn_panning_off(platform_state);
                    platform_state->is_space_down = false;
                }
            } break;
        case SDL_WINDOWEVENT:
            {
                if (platform_state->window_id != event.window.windowID)
                {
                    break;
                }
                switch (event.window.event)
                {
                    // Just handle every event that changes the window size.
                case SDL_WINDOWEVENT_MOVED:
                    platform_state->num_point_results = 0;
                    platform_state->num_pressure_results = 0;
                    platform_state->is_pointer_down = false;
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    platform_state->width = event.window.data1;
                    platform_state->height = event.window.data2;
                    set_flag(input_flags, MiltonInputFlags_FULL_REFRESH);
                    glViewport(0, 0, platform_state->width, platform_state->height);
                    break;
                case SDL_WINDOWEVENT_LEAVE:
                    if ( event.window.windowID != platform_state->window_id )
                    {
                        break;
                    }
                    if ( platform_state->is_pointer_down )
                    {
                        // Not enough info..
                        pointer_up = true;
                        //platform_state->is_pointer_down = false;
                        //set_flag(input_flags, MiltonInputFlags_END_STROKE);
                    }
                    cursor_show();
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
        default:
            break;
        }
#if defined(_MSC_VER)
#pragma warning (pop)
#endif
        if ( platform_state->should_quit )
        {
            break;
        }
    }  // ---- End of SDL event loop

    if ( pointer_up )
    {
        // Add final point
        if ( !platform_state->is_panning && platform_state->is_pointer_down )
        {
            set_flag(input_flags, MiltonInputFlags_END_STROKE);
            input_point = { event.button.x, event.button.y };

            if (platform_state->num_point_results < MAX_INPUT_BUFFER_ELEMS)
            {
                milton_input.points[platform_state->num_point_results++] = input_point;
            }
            // Start drawing hover as soon as we stop the stroke.
            milton_input.hover_point = input_point;
            set_flag(input_flags, MiltonInputFlags_HOVERING);
        }
        else if ( platform_state->is_panning && !platform_state->panning_locked )
        {
            if (!platform_state->is_space_down)
            {
                platform_state->is_panning = false;
                platform_state->stopped_panning = true;
            }
        }
        platform_state->is_pointer_down = false;
    }

    if (platform_state->panning_fsm == PanningFSM_MOUSE_PANNING)
    {
        platform_state->num_point_results = 0;
    }

    milton_input.flags = (MiltonInputFlags)input_flags;

    return milton_input;
}

// ---- milton_main

int milton_main(MiltonStartupFlags startup_flags)
{
    // TODO: windows scaling support
#if 0
#if defined(_WIN32)
    // TODO: Try to load DLL instead of linking directly
    SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
#endif
#endif
    // Note: Possible crash regarding SDL_main entry point.
    // Note: Event handling, File I/O and Threading are initialized by default
    SDL_Init(SDL_INIT_VIDEO);

    PlatformState platform_state = {};

    PlatformPrefs prefs = {};

    milton_prefs_load(&prefs);

    if (prefs.width == 0 || prefs.height == 0)
    {
        platform_state.width = 1280;
        platform_state.height = 800;
    }
    else
    {
        platform_state.width = prefs.width;
        platform_state.height = prefs.height;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_Window* window = SDL_CreateWindow("Milton",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          platform_state.width, platform_state.height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        milton_log("[ERROR] -- Exiting. SDL could not create window\n");
        exit(EXIT_FAILURE);
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if (!gl_context)
    {
        milton_fatal("Could not create OpenGL context\n");
    }

    SDL_GL_SetSwapInterval(1);

    platform_load_gl_func_pointers();

    milton_log("Created OpenGL context with version %s\n", glGetString(GL_VERSION));
    milton_log("    and GLSL %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    // ==== Initialize milton
    //  Total (static) memory requirement for Milton
#if MILTON_DEBUG
    size_t sz_root_arena = (size_t)1 * 1024 * 1024 * 1024;
#else
    size_t sz_root_arena = (size_t)10 * 1024 * 1024;  // 10 MB is roughly the non-dynamic required memory for Milton
#endif

    // Using platform_allocate because stdlib calloc will be really slow.
    void* big_chunk_of_memory = platform_allocate_bounded_memory(sz_root_arena);

    if (!big_chunk_of_memory)
    {
        milton_fatal("Could allocate bounded virtual memory for Milton.\n");
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
    if (SDL_GetWindowWMInfo( window, &sysinfo ))
    {
        switch(sysinfo.subsystem)
        {
#if defined(_WIN32)
        case SDL_SYSWM_WINDOWS:
            {
                // Load EasyTab
                EasyTab_Load(sysinfo.info.win.window);
                { // Handle the case where the window was too big for the screen.
                    HWND hwnd = sysinfo.info.win.window;
                    RECT res_rect;
                    RECT win_rect;
                    HWND dhwnd = GetDesktopWindow();
                    GetWindowRect(dhwnd, &res_rect);
                    GetClientRect(hwnd, &win_rect);

                    platform_state.hwnd = hwnd;

                    i32 snap_threshold = 300;
                    if ( win_rect.right  != platform_state.width ||
                         win_rect.bottom != platform_state.height
                         // Also maximize if the size is large enough to "snap"
                         || (win_rect.right + snap_threshold >= res_rect.right &&
                             win_rect.left + snap_threshold >= res_rect.left))
                    {
                        // By this point we can assume that our prefs weren't right. Let's maximize.
                        SetWindowPos(hwnd, HWND_TOP, 100,100, win_rect.right-100, win_rect.bottom -100, SWP_SHOWWINDOW);
                        platform_state.width = win_rect.right - 100;
                        platform_state.height = win_rect.bottom - 100;
                        ShowWindow(hwnd, SW_MAXIMIZE);
                    }
                }
                break;
            }
#elif defined(__linux__)
        case SDL_SYSWM_X11:
            EasyTab_Load(sysinfo.info.x11.display, sysinfo.info.x11.window);
            break;
#endif
        default:
            break;
        }
    }
    else
    {
        milton_die_gracefully("Can't get system info!\n");
    }
#if defined(_MSC_VER)
#pragma warning (pop)
#endif

    milton_resize(milton_state, {}, {platform_state.width, platform_state.height});

    platform_state.window_id = SDL_GetWindowID(window);

    // Every X ms, call this callback to send us an event so we don't wait for user input.
    // Called periodically to force updates that don't depend on user input.
    SDL_AddTimer(100,
                 [](u32 interval, void *param)
                 {
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

    // Init ImGUI
    //ImGui_ImplSdl_Init(window);
    ImGui_ImplSdlGL3_Init(window);

#if defined(_WIN32)
    {  // Load icon (Win32)
        int si = sizeof(HICON);
        HINSTANCE handle = GetModuleHandle(nullptr);
        char icopath[MAX_PATH] = "milton_icon.ico";
        platform_fname_at_exe(icopath, MAX_PATH);
        HICON icon = (HICON)LoadImageA(NULL, icopath, IMAGE_ICON, /*W*/0, /*H*/0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
        if (icon != NULL)
        {
            SendMessage(platform_state.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        }
    }
    {  // Set brush HW cursor
        size_t w = (size_t)GetSystemMetrics(SM_CXCURSOR);
        size_t h = (size_t)GetSystemMetrics(SM_CYCURSOR);

        size_t arr_sz = (w*h+7) / 8;

        char* andmask = arena_alloc_array(&root_arena, arr_sz, char);
        char* xormask = arena_alloc_array(&root_arena, arr_sz, char);

        b32 toggle_black = true;
        {
            size_t cx = w/2;
            size_t cy = h/2;
            for (size_t j = 0; j < h; ++j)
            {
                for (size_t i = 0; i < w; ++i)
                {
                    size_t dist = (i-cx)*(i-cx) + (j-cy)*(j-cy);

                    // 32x32 default;
                    i64 girth = 5; // girth of cursor in pixels
                    size_t radius;
                    if (w == 32 && h == 32)
                    {
                        // 32x32
                        radius = 9;
                    }
                    else if (w == 64 && h == 64)
                    {
                        girth = 8;
                        radius = 20;
                    }
                    else
                    {
                        radius = 9;
                    }
                    size_t radiussq = radius*radius;
                    i64 girthsq = girth*girth;
                    i64 diff = (i64)(dist - radiussq);
                    b32 incircle = diff < girthsq && diff > -girthsq;

                    size_t idx = j*w + i;

                    size_t ai = idx / 8;
                    size_t bi = idx % 8;

                    if (incircle
                        && (i > cx-radius/2 && i < cx+radius/2 || j > cy-radius/2 && j < cy+radius/2))
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
                }
            }
        }

        platform_state.hcursor = CreateCursor(/*HINSTANCE*/ 0,
                                              /*xHotSpot*/(int)(w/2),
                                              /*yHotSpot*/(int)(h/2),
                                              /* nWidth */(int)w,
                                              /* nHeight */(int)h,
                                              (VOID*)andmask,
                                              (VOID*)xormask);

        if (platform_state.hcursor != NULL)
        {
            platform_state.setting_hcursor = true;
        }

        // TODO: Create cursor for other platforms when porting
        //  Use the SDL call? It's not documented but we allready did the
        //  work for CreateCursor and the function signatures are very
        //  similar
    }
#endif


    // ImGui setup
    {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;  // Don't save any imgui.ini file
        char fname[MAX_PATH] = "carlito.ttf";
        platform_fname_at_exe(fname, MAX_PATH);
        FILE* fd_sentinel = fopen(fname, "rb");

        if (fd_sentinel)
        {
            fclose(fd_sentinel);
            ImFont* im_font =  io.Fonts->ImFontAtlas::AddFontFromFileTTF(fname, 14);
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

    while (!platform_state.should_quit)
    {

        u32 frame_start_ms = SDL_GetTicks();

        ImGuiIO& imgui_io = ImGui::GetIO();

        MiltonInput milton_input = sdl_event_loop(milton_state, &platform_state);

        // TODO: Is a static the best choice?
        static b32 first_run = true;
        if ( first_run )
        {
            first_run = false;
            milton_input.flags = MiltonInputFlags_FULL_REFRESH;
        }

        if ( platform_state.stopped_panning )
        {
            milton_state->flags |= MiltonStateFlags_REQUEST_QUALITY_REDRAW;
            platform_state.panning_fsm = PanningFSM_NOTHING;
        }

        {
            int x = 0;
            int y = 0;
            SDL_GetMouseState(&x, &y);

            // Handle system cursor and platform state related to current_mode
            if (platform_state.is_panning)
            {
                cursor_set_and_show(platform_state.cursor_sizeall);
            }
            else if (platform_state.stopped_panning)
            {
                milton_state->flags |= MiltonStateFlags_REQUEST_QUALITY_REDRAW;
                cursor_set_and_show(platform_state.cursor_default);
            }
            else if (milton_state->current_mode == MiltonMode_EXPORTING)
            {
                cursor_set_and_show(platform_state.cursor_crosshair);
                platform_state.was_exporting = true;
            }
            else if (platform_state.was_exporting)
            {
                cursor_set_and_show(platform_state.cursor_default);
                platform_state.was_exporting = false;
            }
            else if (milton_state->current_mode == MiltonMode_EYEDROPPER)
            {
                cursor_set_and_show(platform_state.cursor_crosshair);
                platform_state.is_pointer_down = false;
            }
            else if ( milton_state->gui->visible &&
                      is_inside_rect_scalar(get_bounds_for_picker_and_colors(&milton_state->gui->picker), x,y) )
            {
                cursor_set_and_show(platform_state.cursor_default);
            }
            else if ( ImGui::GetIO().WantCaptureMouse )
            {
                cursor_set_and_show(platform_state.cursor_default);
            }
            else if (milton_state->current_mode == MiltonMode_PEN || milton_state->current_mode == MiltonMode_ERASER )
            {
                if (platform_state.hcursor != NULL && platform_state.setting_hcursor)
                {
                    SetCursor(platform_state.hcursor);
                }
            }
            else if (milton_state->current_mode != MiltonMode_PEN || milton_state->current_mode != MiltonMode_ERASER )
            {
                cursor_hide();
            }

            // Show resize icon
            int pad = 20;
            if (x > milton_state->view->screen_size.w - pad   ||
                x < pad                                       ||
                y > milton_state->view->screen_size.h - pad   ||
                y < pad)
            {
                cursor_set_and_show(platform_state.cursor_default);
                platform_state.setting_hcursor = false;
            }
            else
            {
                platform_state.setting_hcursor = true;
            }
        }

        // IN OSX: SDL polled all events, we get all the pressure inputs from our hook
#if defined(__MACH__)
        assert( num_pressure_results == 0 );
        int num_polled_pressures = 0;
        float* polled_pressures = milton_osx_poll_pressures(&num_polled_pressures);
        if ( num_polled_pressures )
        {
            for (int i = num_polled_pressures - 1; i >= 0; --i)
            {
                milton_input.pressures[num_pressure_results++] = polled_pressures[i];
            }
        }
#endif

        i32 input_flags = (i32)milton_input.flags;

        ImGui_ImplSdlGL3_NewFrame();
        // Clear our pointer input because we captured an ImGui widget!
        if (ImGui::GetIO().WantCaptureMouse)
        {
            platform_state.num_point_results = 0;
            platform_state.is_pointer_down = false;
            set_flag(input_flags, MiltonInputFlags_IMGUI_GRABBED_INPUT);
        }

        milton_imgui_tick(&milton_input, &platform_state, milton_state);

        // Clear pan delta if we are zooming
        if ( milton_input.scale != 0 )
        {
            milton_input.pan_delta = {};
            set_flag(input_flags, MiltonInputFlags_FAST_DRAW);
        }
        else
        {
            if (platform_state.is_panning)
            {
                set_flag(input_flags, MiltonInputFlags_PANNING);
                platform_state.num_point_results = 0;
            }
        }

#if 0
        milton_log ("#Pressure results: %d\n", num_pressure_results);
        milton_log ("#   Point results: %d\n", num_point_results);
#endif

        // Mouse input, fill with NO_PRESSURE_INFO
        if ( platform_state.num_pressure_results == 0 )
        {
            for (int i = platform_state.num_pressure_results; i < platform_state.num_point_results; ++i)
            {
                milton_input.pressures[platform_state.num_pressure_results++] = NO_PRESSURE_INFO;
            }
        }

        if ( platform_state.num_pressure_results < platform_state.num_point_results )
        {
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
             platform_state.height != milton_state->view->screen_size.y )
        {
            milton_resize(milton_state, pan_delta, {platform_state.width, platform_state.height});
        }
        milton_input.pan_delta = pan_delta;


        // ---- Recording. No more messing with milton_input after this.

        if ( startup_flags.history_debug == HistoryDebug_RECORD )
        {
            history_debugger_append(&milton_input);
        }

        platform_state.pan_start = platform_state.pan_point;
        // ==== Update and render
        //SDL_Delay(17);
        milton_update(milton_state, &milton_input);
        if ( !(milton_state->flags & MiltonStateFlags_RUNNING) )
        {
            platform_state.should_quit = true;
        }
        milton_gl_backend_draw(milton_state);
        ImGui::Render();
        SDL_GL_SwapWindow(window);
        SDL_WaitEvent(NULL);

#if MILTON_DEBUG
        milton_state->DEBUG_last_frame_time = SDL_GetTicks() - frame_start_ms;
#endif
    }

    EasyTab_Unload();

    // Release pages. Not really necessary but we don't want to piss off leak
    // detectors, do we?
    platform_deallocate(big_chunk_of_memory);

    { // Set platform prefs
        prefs.width  = platform_state.width;
        prefs.height = platform_state.height;
    }
    milton_prefs_save(&prefs);

    SDL_Quit();

    return 0;
}

