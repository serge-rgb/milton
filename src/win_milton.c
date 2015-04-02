#include "system_includes.h"

#include "libserg/memory.h"

#ifdef _MSC_VER
#if (_MSC_VER == 1800)
#define inline __inline
#endif
#endif

#include "milton.h"

#define snprintf sprintf_s

typedef struct
{
    HWND window;
    // Window dimensions:
    int32_t width;
    int32_t height;
    BITMAPINFO bitmap_info;
    v2l stored_brush;
} Win32State;

typedef enum
{
    GuiMsg_RESIZED     = (1 << 0),
    GuiMsg_SHOULD_QUIT = (1 << 1),
} GuiMsg;

typedef struct
{
    int32 width;
    int32 height;
    // Mouse info
    int32 mouse_x;
    int32 mouse_y;
    bool32 left_down;
    bool32 right_down;
} GuiData;

static GuiMsg  g_gui_msgs;
static GuiData g_gui_data;

void platform_quit()
{
    g_gui_msgs |= GuiMsg_SHOULD_QUIT;
}

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

static void win32_resize(
        Win32State* win_state,
        int32 width, int32 height)
{
    // NOTE: here we would free the raster buffer

    win_state->width = width;
    win_state->height = height;

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

    // NOTE: Here we would allocate a new raster buffer
}

static MiltonInput win32_process_input(Win32State* win_state, HWND window)
{
    MiltonInput input = { 0 };
    MSG message;
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
                g_gui_data.mouse_x   = GET_X_LPARAM(message.lParam);
                g_gui_data.mouse_y   = GET_Y_LPARAM(message.lParam);
                g_gui_data.left_down = 1;

#if 0
                char buffer[1024];
                snprintf(buffer, 1024, "Click! %d %d\n", g_gui_data.mouse_x, g_gui_data.mouse_y);
                OutputDebugStringA(buffer);
#endif
                break;
            }
        case WM_LBUTTONUP:
            {
                g_gui_data.left_down = 0;
                break;
            }
        case WM_MOUSEMOVE:
            {
                if (g_gui_data.left_down || g_gui_data.right_down)
                {
                    g_gui_data.mouse_x = GET_X_LPARAM(message.lParam);
                    g_gui_data.mouse_y = GET_Y_LPARAM(message.lParam);
                }
                break;
            }
        case WM_MOUSEWHEEL:
            {
                int delta = GET_WHEEL_DELTA_WPARAM(message.wParam);
                input.scale = delta;
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
                bool32 alt_key_was_down = (message.lParam & (1 << 29));
                if (was_down && vkcode == VK_ESCAPE)
                {
                    platform_quit();
                }
            }
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
            win32_resize(win_state, g_gui_data.width, g_gui_data.height);
            g_gui_msgs ^= GuiMsg_RESIZED;
            input.full_refresh = 1;
        }
    }

    if (g_gui_data.left_down)
    {
        win_state->stored_brush.x = (int64)g_gui_data.mouse_x;
        win_state->stored_brush.y = (int64)g_gui_data.mouse_y;
        input.brush = &win_state->stored_brush;
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
    case WM_SIZE:
        {
            // Let win32_process_input handle it.
            g_gui_msgs |= GuiMsg_RESIZED;
            if (wParam == SIZE_MAXIMIZED)
            {
                g_gui_data.width = LOWORD(lParam);
                g_gui_data.height = HIWORD(lParam);
            }
            if (wParam == SIZE_RESTORED)
            {
                g_gui_data.width = LOWORD(lParam);
                g_gui_data.height = HIWORD(lParam);
            }
            break;
        }
    case WM_CREATE:
        {
            break;
        }
    case WM_DESTROY:
        {
            platform_quit();
            break;
        }
    case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);

            EndPaint(window, &paint);
            break;
        }
    default:
        {
            result = DefWindowProc(window, message, wParam, lParam);
        }
    };
    return result;
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
    int32 width = 1920;
    int32 height = 1080;
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

    Win32State win_state = { 0 };
    {
        win_state.window = window;
    }

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
        milton_init(milton_state);
    }

    // Initialize global GUI data.
    {
        g_gui_data.width = width;
        g_gui_data.height = height;
    }

    MiltonInput input = { 0 };
    {
        input.full_refresh = 1;
    }
    while (!(g_gui_msgs & GuiMsg_SHOULD_QUIT))
    {
        bool32 modified = milton_update(milton_state, &input);
        if (modified)
        {
            win32_display_raster_buffer(
                    &win_state,
                    milton_state->raster_buffer,
                    milton_state->full_width,
                    milton_state->full_height,
                    milton_state->bytes_per_pixel
                    );
        }

        input = win32_process_input(&win_state, window);
        milton_state->screen_size = make_v2l( win_state.width, win_state.height );
        // Sleep until we need to.
        WaitMessage();
    }

    return TRUE;
}
