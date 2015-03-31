#include "system_includes.h"
#include "gl_helpers.h"

#include "libserg/memory.h"

#include "milton.h"

#define snprintf sprintf_s

typedef struct
{
    HWND window;
    // Window dimensions:
    int32_t width;
    int32_t height;
    BITMAPINFO bitmap_info;
} Win32State;

typedef enum
{
    GuiMsg_RESIZING    = (1 << 0),
    GuiMsg_RESIZED     = (1 << 1),
    GuiMsg_SHOULD_QUIT = (1 << 2),
} GuiMsg;

typedef struct
{
    int width;
    int height;
} GuiData;

static GuiMsg  g_gui_msgs;
static GuiData g_gui_data;

void platform_quit()
{
    g_gui_msgs |= GuiMsg_SHOULD_QUIT;
}

void path_at_exe(char* full_path, int buffer_size, char* fname)
{
    GetModuleFileName(NULL, full_path, (DWORD)buffer_size);
    {  // Trim to backslash
        int len = 0;
        for(int i = 0; i < buffer_size; ++i)
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
        int width, int height)
{
    // NOTE: here we would free the raster buffer

    win_state->width = width;
    win_state->height = height;
    //win_state->bytes_per_pixel = 3;

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
                break;
            }
        case WM_LBUTTONUP:
            {
                break;
            }
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_KEYDOWN:
            {
                uint32_t vkcode = (uint32_t)message.wParam;
                bool32 was_down = ((message.lParam & (1 << 30)) != 0);
                bool32 is_down  = ((message.lParam & (1 << 31)) == 0);
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
    case WM_SIZE:
        {
            // Let win32_process_input handle it.
            g_gui_msgs |= GuiMsg_RESIZED;
            break;
        }
    case WM_SIZING:
        {
            RECT* rect = (RECT*)lParam;
            g_gui_data.width = rect->right - rect->left;
            g_gui_data.height = rect->bottom - rect->top;
            break;
        }
    case WM_CREATE:
        {
            break;
        }
    case WM_MOUSEMOVE:
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
        uint8_t* bytes, int32_t width, int32_t height, uint8_t bytes_per_pixel)
{
    // Make sure we have allocated enough memory for the current window.
    assert(width > win_state->width);
    assert(height > win_state->height);

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

    int x = 100;
    int y = 100;
    int width = 1024;
    int height = 768;
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
    Arena frame_arena = arena_spawn(&root_arena, frame_heap_in_MB);
    MiltonState milton_state;
    {
        milton_state.root_arena = &root_arena;
        milton_state.frame_arena = &frame_arena;
        milton_init(&milton_state);
    }

    // Initialize global GUI data.
    {
        g_gui_data.width = width;
        g_gui_data.height = height;
    }

    while (!(g_gui_msgs & GuiMsg_SHOULD_QUIT))
    {
        MiltonInput input = win32_process_input(&win_state, window);

        milton_update(&milton_state, &input);
        win32_display_raster_buffer(
                &win_state,
                milton_state.raster_buffer,
                milton_state.width,
                milton_state.height,
                milton_state.bytes_per_pixel
                );
        // Sleep until we need to.
        WaitMessage();
    }

    return TRUE;
}
