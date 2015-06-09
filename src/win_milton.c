// win_milton.h
// (c) Copyright 2015 by Sergio Gonzalez

#include "system_includes.h"

#include "libserg/memory.h"

#ifdef _MSC_VER
#if (_MSC_VER == 1800)
//#define inline __inline
#define inline __forceinline  // This don't do shit...
#endif
#endif

#define snprintf sprintf_s

#include "milton.h"
#include "platform.h"

typedef struct Win32State_s Win32State;

#include "win32_wacom_defines.h"

struct Win32State_s
{
    HWND window;
    // Window dimensions:
    int32_t width;
    int32_t height;
    BITMAPINFO bitmap_info;
    v2i input_pointer;

    HINSTANCE wintab_handle;
    WacomAPI wacom;
    POINT wacom_pt_old;
    POINT wacom_pt_new;
    UINT wacom_prs_old;
    UINT wacom_prs_new;
    UINT wacom_prs_max;
    HCTX wacom_ctx;
    int wacom_num_attached_devices;
};

#include "win32_wacom.h"

typedef enum
{
    GuiMsg_RESIZED              = (1 << 0),
    GuiMsg_SHOULD_QUIT          = (1 << 1),
    GuiMsg_END_STROKE           = (1 << 2),
    GuiMsg_GL_DRAW              = (1 << 3),
    GuiMsg_CHANGE_SCALE_CENTER  = (1 << 4),
} GuiMsg;

typedef struct
{
    // Mouse info
    int32 pointer_x;
    int32 pointer_y;
    bool32 is_stroking;
    bool32 right_down;
    bool32 is_panning;
    v2i old_pan;
} GuiData;

static HGLRC   g_gl_context_handle;
static GuiMsg  g_gui_msgs;
static GuiData g_gui_data;

void platform_quit()
{
    g_gui_msgs |= GuiMsg_SHOULD_QUIT;
}

// Copied from libserg.
// Defined below.
static int milton_win32_setup_context(HWND window, HGLRC* context);


static void path_at_exe(char* full_path, int32 buffer_size, char* fname)
{
    GetModuleFileName(NULL, full_path, (DWORD)buffer_size);
    {  // Trim to backslash
        int32 len = 0;
        for(int32 i = 0; i < buffer_size; ++i)
        {
            char c = full_path[i];
            if (!c)
                break;
            if (c == '\\')
                len = i;
        }
        full_path[len + 1] = '\0';
    }

    strcat(full_path, fname);
}

void win32_log(char *format, ...)
{
	char message[ 128 ];

	int num_bytes_written = 0;

	va_list args;

	assert ( format );

	va_start( args, format );

	num_bytes_written = _vsnprintf(message, sizeof( message ) - 1, format, args);

	if ( num_bytes_written > 0 )
	{
            OutputDebugStringA( message );
	}

	va_end( args );
}

static void win32_resize(Win32State* win_state)
{
    // NOTE: here we would free the raster buffer

    BITMAPINFOHEADER header;
    {
        header.biSize = sizeof(win_state->bitmap_info.bmiHeader);
        header.biWidth = win_state->width;
        header.biHeight = -win_state->height;
        header.biPlanes = 1;
        header.biBitCount = 32;
        header.biCompression = BI_RGB;
        header.biSizeImage = 0;
        header.biXPelsPerMeter = 0;
        header.biYPelsPerMeter = 0;
        header.biClrUsed = 0;
        header.biClrImportant = 0;
    }
    win_state->bitmap_info.bmiHeader = header;

    RECT rect;
    GetClientRect(win_state->window, &rect);
    win_state->width = (int)(rect.right - rect.left);
    win_state->height = (int)(rect.bottom - rect.top);

    glViewport(0, 0, win_state->width, win_state->height);
}


static MiltonInput win32_process_input(Win32State* win_state, HWND window)
{
    MiltonInput input = { 0 };
    MSG message;
    bool32 is_ctrl_down = GetKeyState(VK_CONTROL) >> 1;
#if 0
    if (win_state->wintab_handle)
    {
        PACKET packet = { 0 };
        while (win_state->wacom.WTPacketsGet(win_state->wacom_ctx, 1, &packet))
        {

        }
    }
#endif
    while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            platform_quit();
        }
        switch (message.message)
        {
        case WM_LBUTTONDOWN:
            {
                g_gui_data.pointer_x   = GET_X_LPARAM(message.lParam);
                g_gui_data.pointer_y   = GET_Y_LPARAM(message.lParam);
                g_gui_data.is_stroking = true;
                TRACKMOUSEEVENT track_mouse_event = { 0 };
                {
                    track_mouse_event.cbSize = sizeof(TRACKMOUSEEVENT);
                    track_mouse_event.dwFlags = TME_LEAVE;
                    track_mouse_event.hwndTrack = win_state->window;

                }
                TrackMouseEvent(&track_mouse_event);
                break;
            }
        case WM_MOUSELEAVE:
            // Fall-through
        case WM_LBUTTONUP:
            {
                if (g_gui_data.is_stroking)
                {
                    g_gui_msgs |= GuiMsg_END_STROKE;
                }
                g_gui_data.is_stroking = false;
                break;
            }
        case WM_MOUSEMOVE:
            {
                g_gui_data.pointer_x = GET_X_LPARAM(message.lParam);
                g_gui_data.pointer_y = GET_Y_LPARAM(message.lParam);
                break;
            }
        case WM_MOUSEWHEEL:
            {
                int delta = GET_WHEEL_DELTA_WPARAM(message.wParam);
                input.scale = delta;
                g_gui_msgs |= GuiMsg_CHANGE_SCALE_CENTER;

                break;
            }
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_KEYDOWN:
            {
                uint32_t vkcode = (uint32_t)message.wParam;
                bool32 was_down = ((message.lParam & (1 << 30)) != 0);
                bool32 is_down  = ((message.lParam & ((uint64_t)1 << 31)) == 0);
                bool32 alt_key_was_down     = (message.lParam & (1 << 29));
                if (was_down && vkcode == VK_ESCAPE)
                {
                    platform_quit();
                }
                if (was_down && vkcode == VK_BACK)
                {
                    input.flags |= MiltonInputFlags_RESET;
                }

                assert ('Z' == 0x5A);
                if (was_down && is_ctrl_down && vkcode == 'Z')
                {
                    input.flags |= MiltonInputFlags_UNDO;
                    input.flags |= MiltonInputFlags_FULL_REFRESH;
                }
                if (was_down && is_ctrl_down && vkcode == 'R')
                {
                    input.flags |= MiltonInputFlags_REDO;
                    input.flags |= MiltonInputFlags_FULL_REFRESH;
                }
                if (was_down && vkcode == VK_SPACE)

                {
                    input.flags |= MiltonInputFlags_FULL_REFRESH;
                }
#if 0
                if (is_down && vkcode == VK_LEFT)
                {
                    input.rotation = 5;
                }
                if (is_down && vkcode == VK_RIGHT)
                {
                    input.rotation = -5;
                }
#endif
            }
        // Wacom support
#if 0
        case WT_PACKET:
            {
                HCTX hctx = (HCTX)message.lParam;
                PACKET pkt;

                // Wintab X/Y data is in screen coordinates. These will have to
                // be converted to client coordinates in the WM_PAINT handler.
                if (win_state->wacom.WTPacket(hctx, (UINT)message.wParam, &pkt))
                {
                    win32_log("Render point for hctx: 0x%X\n", hctx);

                    win_state->wacom_pt_old = win_state->wacom_pt_new;
                    win_state->wacom_prs_old = win_state->wacom_prs_new;

                    win_state->wacom_pt_new.x = pkt.pkX;
                    win_state->wacom_pt_new.y = pkt.pkY;

                    win_state->wacom_prs_new = pkt.pkNormalPressure;

                    if (win_state->wacom_pt_new.x != win_state->wacom_pt_old.x ||
                            win_state->wacom_pt_new.y != win_state->wacom_pt_old.y ||
                            win_state->wacom_prs_new != win_state->wacom_prs_old)
                    {
                        win_state->input_pointer.x = (int64)g_gui_data.pointer_x;
                        win_state->input_pointer.y = (int64)g_gui_data.pointer_y;
                    }
                }
                else
                {
                    win32_log("Oops - got pinged by an unknown context: 0x%X", hctx);
                }
                break;
            }
        case WT_INFOCHANGE:
            {
                assert (!"We don't support this yet");
#if 0
                int num_attached_devices = 0;
                win_state->wacom.WTInfoA(WTI_INTERFACE, IFC_NDEVICES, &num_attached_devices);

                win32_log("WT_INFOCHANGE detected; number of connected tablets is now: %i\n", num_attached_devices);

                if (num_attached_devices != win_state->wacom_num_attached_devices )
                {
                    // kill all current tablet contexts
                    CloseTabletContexts();

                    // Add some delay to give driver a chance to catch up in configuring
                    // to the current attached tablets.
                    // 1 second seems to work - your mileage may vary...
                    ::Sleep(1000);

                    // re-enumerate attached tablets
                    OpenTabletContexts(hWnd);
                }

                if ( win_state->wacom_num_attached_devices == 0 )
                {
                    ShowError("No tablets found.");
                    SendMessage(hWnd, WM_DESTROY, 0, 0L);
                }
#endif
                break;
            }
        case WT_PROXIMITY:
            {
#if 0
                hctx = (HCTX)wParam;

                bool32 entering = (HIWORD(lParam) != 0);

                if ( entering )
                {
                    if ( gContextMap.count(hctx) > 0 )
                    {
                        TabletInfo info = gContextMap[hctx];
                    }
                    else
                    {
                        win32_log("Oops - couldn't find context: 0x%X\n", hctx);
                    }
                }
#endif
                break;
            }
			break;
#endif
        default:
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
                break;
            }
        }
        // Handle custom flags
        if (g_gui_msgs & GuiMsg_RESIZED)
        {
            win32_resize(win_state);
            g_gui_msgs ^= GuiMsg_RESIZED;
            input.flags |= MiltonInputFlags_FULL_REFRESH;
        }
        if (g_gui_msgs & GuiMsg_END_STROKE)
        {
            input.flags |= MiltonInputFlags_END_STROKE;
            g_gui_msgs ^= GuiMsg_END_STROKE;
        }
    }
    if (is_ctrl_down && g_gui_data.is_stroking)  // CTRL is down.
    {
        if (
                win_state->input_pointer.x != g_gui_data.pointer_x ||
                win_state->input_pointer.y != g_gui_data.pointer_y
                )
        {
            win_state->input_pointer.x = (int64)g_gui_data.pointer_x;
            win_state->input_pointer.y = (int64)g_gui_data.pointer_y;
            v2i new_pan = win_state->input_pointer;
            // Just started panning.
            if (!g_gui_data.is_panning)
            {
                input.pan_delta = (v2i) { 0 };
            }
            else
            {
                input.pan_delta = sub_v2i(new_pan, g_gui_data.old_pan);
            }
            g_gui_data.is_panning = true;
            g_gui_data.old_pan = new_pan;
            input.flags |= MiltonInputFlags_FULL_REFRESH;
        }

    }
    else
    {
        g_gui_data.is_panning = false;
    }

    if (!is_ctrl_down && g_gui_data.is_stroking)
    {
        if (
                win_state->input_pointer.x != g_gui_data.pointer_x ||
                win_state->input_pointer.y != g_gui_data.pointer_y
                )
        {
            win_state->input_pointer.x = (int64)g_gui_data.pointer_x;
            win_state->input_pointer.y = (int64)g_gui_data.pointer_y;
            input.point = &win_state->input_pointer;
        }
    }
    return input;
}

LRESULT APIENTRY WndProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
        )
{
    LRESULT result = 0;
    switch (message)
    {
    case WM_CREATE:
        {
            bool32 success = milton_win32_setup_context(window, &g_gl_context_handle);
            if (!success)
            {
                platform_quit();
            }

            break;
        }
    case WM_DESTROY:
        {
            platform_quit();
            break;
        }
	case WM_SIZE:
	{
		// Let win32_process_input handle it.
		g_gui_msgs |= GuiMsg_RESIZED;
		break;
	}
#if 0
    case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);

            EndPaint(window, &paint);
            break;
        }
#endif
    default:
        {
            result = DefWindowProc(window, message, wParam, lParam);
        }
    };
    return result;
}

void CALLBACK win32_fire_timer(HWND window, UINT uMsg, UINT_PTR event_id, DWORD time)
{
    if (event_id == 42)
    {
        SetTimer(window, 42, 8/*ms*/, win32_fire_timer);
    }

    g_gui_msgs |= GuiMsg_GL_DRAW;
}

static void win32_display_raster_buffer(
        Win32State* win_state,
        uint8_t* bytes, int32_t full_width, int32_t full_height, uint8_t bytes_per_pixel)
{
    // Make sure we have allocated enough memory for the current window.
    assert(full_width > win_state->width);
    assert(full_height > win_state->height);

    StretchDIBits(
            GetDC(win_state->window),
            // Dest:
            0, 0, win_state->width, win_state->height,
            // Src:
            0, 0, win_state->width, win_state->height,
            // Data:
            bytes,
            &win_state->bitmap_info,
            DIB_RGB_COLORS,
            SRCCOPY);
}

#pragma warning(suppress: 28251)
int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
{
    Win32State win_state = { 0 };

    if (!win32_wacom_load_wintab(&win_state))
    {
        OutputDebugStringA("No wintab.\n");
    }


    WNDCLASS window_class = { 0 };

    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = WndProc;
    window_class.hInstance = hInstance;
    window_class.hIcon = NULL;
    window_class.hCursor = LoadCursor(0, IDC_ARROW);
    window_class.lpszClassName = "MiltonClass";

    if (!RegisterClassA(&window_class))
    {
        return FALSE;
    }

    int32 x = 100;
    int32 y = 100;
    //int32 width = 1920;
    //int32 height = 1080;
    int32 width = 1280;
    int32 height = 800;

    HWND window = CreateWindowExA(
            0, //WS_EX_TOPMOST ,  // dwExStyle
            window_class.lpszClassName,     // class Name
            "Milton",                      // window name
            //WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE | WS_POPUP,          // dwStyle
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            // WS_OVERLAPPED | WS_CAPTION | WS_BORDER | WS_VISIBLE | WS_SYSMENU,
            //WS_VISIBLE | WS_BORDER | WS_POPUP,
            x,                      // x
            y,                      // y
            width,                  // width
            height,                 // height
            0,                                  // parent
            0,                                  // menu
            hInstance,                          // hInstance
            0                                   // lpParam
            );

    if (!window)
    {
        return FALSE;
    }
    else
    {
        win_state.window = window;
    }

    win32_wacom_get_context(&win_state);

    const size_t total_memory_size = 1 * 1024 * 1024 * 1024;  // Total memory requirement for Milton.
    const size_t frame_heap_in_MB = 32 * 1024 * 1024;         // Size of transient memory

    // Create root arena
    void* big_chunk_of_memory =
        VirtualAlloc(
                // Specify an adress on debug mode
#ifndef NDEBUG
                (LPVOID)(1024LL * 1024 * 1024 * 1024), //  lpAddress,
#else
                NULL, //  lpAddress,
#endif
                total_memory_size,//  dwSize,
                MEM_COMMIT | MEM_RESERVE, //  flAllocationType,
                PAGE_READWRITE//  flProtect
                );

    assert (big_chunk_of_memory);
    Arena root_arena = arena_init(big_chunk_of_memory, total_memory_size);
    // Create a transient arena, called once per update
    Arena transient_arena = arena_spawn(&root_arena, frame_heap_in_MB);
    MiltonState* milton_state = arena_alloc_elem(&root_arena, MiltonState);
    {
        milton_state->root_arena = &root_arena;
        milton_state->transient_arena = &transient_arena;

        // This assertion basically says that WM_CREATE should have been called
        // by this time.  Since the windows build is not intended to ship, we
        // can do this with a clear conscience.
        assert ( g_gl_context_handle != NULL );

        milton_init(milton_state);
    }

    MiltonInput input = { 0 };
    {
        input.flags |= MiltonInputFlags_FULL_REFRESH;
    }
    win32_resize(&win_state);
    v2i screen_size = { win_state.width, win_state.height };
    milton_resize(milton_state, (v2i){0}, screen_size);

    platform_update_view(
            milton_state->view,
            screen_size,
            (v2i) { 0 });
    SetTimer(window, 42, 16/*ms*/, win32_fire_timer);

    bool32 modified = false;
    while (!(g_gui_msgs & GuiMsg_SHOULD_QUIT))
    {
        v2i screen_size = { win_state.width, win_state.height };

        if (screen_size.w != milton_state->view->screen_size.w ||
            screen_size.h != milton_state->view->screen_size.h ||
            input.pan_delta.w ||
            input.pan_delta.h)
        {
            milton_resize(milton_state, input.pan_delta, screen_size);
        }

        milton_update(milton_state, &input);
        if (g_gui_msgs & GuiMsg_GL_DRAW)
        {
            g_gui_msgs ^= GuiMsg_GL_DRAW;

            HDC dc = GetDC(window);
            milton_gl_backend_draw(milton_state);
            SwapBuffers(dc);
            modified = false;
        }

        input = win32_process_input(&win_state, window);
#if 0
        if (g_gui_msgs & GuiMsg_CHANGE_SCALE_CENTER)
        {
            g_gui_msgs &= ~GuiMsg_CHANGE_SCALE_CENTER;
            milton_state->view->scale_center = win_state.input_pointer;
            input.full_refresh = true;
        }
#endif

        // Sleep until we need to.
        WaitMessage();
    }

    return TRUE;
}

static int milton_win32_setup_context(HWND window, HGLRC* context)
{
    int format_index = 0;

    PIXELFORMATDESCRIPTOR pfd = { 0 };
    {
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags =
            PFD_DRAW_TO_WINDOW |   // support window
            PFD_SUPPORT_OPENGL |   // support OpenGL
            PFD_DOUBLEBUFFER;      // double buffered
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;
    }


    // get the best available match of pixel format for the device context
    format_index = ChoosePixelFormat(GetDC(window), &pfd);

    // make that the pixel format of the device context
    int succeeded = SetPixelFormat(GetDC(window), format_index, &pfd);

    if (!succeeded)
    {
        OutputDebugStringA("Could not set pixel format\n");
        return 0;
    }

    HGLRC dummy_context = wglCreateContext(GetDC(window));
    if (!dummy_context)
    {
        OutputDebugStringA("Could not create GL context. Exiting");
        return 0;
    }
    wglMakeCurrent(GetDC(window), dummy_context);
    if (!succeeded)
    {
        OutputDebugStringA("Could not set current GL context. Exiting");
        return 0;
    }

    GLenum glew_result = glewInit();
    if (glew_result != GLEW_OK)
    {
        OutputDebugStringA("Could not init glew.\n");
        return 0;
    }

    const int pixel_attribs[] =
    {
        WGL_ACCELERATION_ARB   , WGL_FULL_ACCELERATION_ARB           ,
        WGL_DRAW_TO_WINDOW_ARB , GL_TRUE           ,
        WGL_SUPPORT_OPENGL_ARB , GL_TRUE           ,
        WGL_DOUBLE_BUFFER_ARB  , GL_TRUE           ,
        WGL_PIXEL_TYPE_ARB     , WGL_TYPE_RGBA_ARB ,
        WGL_COLOR_BITS_ARB     , 32                ,
        WGL_DEPTH_BITS_ARB     , 24                ,
        WGL_STENCIL_BITS_ARB   , 8                 ,
        0                      ,
    };
    UINT num_formats = 0;

    int format_indices[20];

    wglChoosePixelFormatARB(GetDC(window),
            pixel_attribs, NULL, 20 /*max_formats*/, format_indices, &num_formats);
    if (!num_formats)
    {
        OutputDebugStringA("Could not choose pixel format. Exiting.");
        return 0;
    }

    // The spec *guarantees* that this does not happen but nothing ever works...
    if (num_formats > 20)
    {
        num_formats = 20;
    }

    succeeded = 0;
    for (uint32_t i = 0; i < num_formats; ++i)
    {
        int local_index = format_indices[i];
        succeeded = SetPixelFormat(GetDC(window), local_index, &pfd);
        if (succeeded)
            break;
    }
    if (!succeeded)
    {
        OutputDebugStringA("Could not set pixel format for final rendering context.\n");
        return 0;
    }

    const int context_attribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };
    *context = wglCreateContextAttribsARB(GetDC(window), 0/*shareContext*/,
            context_attribs);
    wglMakeCurrent(GetDC(window), *context);
    return 1;
}
