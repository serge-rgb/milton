#include "system_includes.h"
#include "gl_helpers.h"


#include "milton.h"

typedef int32_t bool32;

#define snprintf sprintf_s

static HGLRC g_glcontext_handle;
bool32 g_running = 1;

void platform_quit()
{
    g_running = 0;
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

// Will setup an OpenGL 3.3 core profile context.
// Loads functions with GLEW
static void win32_setup_context(HWND window, HGLRC* context)
{
    int format_index = 0;

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd
        1,                     // version number
        PFD_DRAW_TO_WINDOW |   // support window
            PFD_SUPPORT_OPENGL |   // support OpenGL
            PFD_DOUBLEBUFFER,      // double buffered
        PFD_TYPE_RGBA,         // RGBA type
        32,                    // 32-bit color depth
        0, 0, 0, 0, 0, 0,      // color bits ignored
        0,                     // no alpha buffer
        0,                     // shift bit ignored
        0,                     // no accumulation buffer
        0, 0, 0, 0,            // accum bits ignored
        24,                    // 24-bit z-buffer
        8,                     // 8-bit stencil buffer
        0,                     // no auxiliary buffer
        PFD_MAIN_PLANE,        // main layer
        0,                     // reserved
        0, 0, 0                // layer masks ignored
    };

    // get the best available match of pixel format for the device context
    format_index = ChoosePixelFormat(GetDC(window), &pfd);

    // make that the pixel format of the device context
    bool32 succeeded = SetPixelFormat(GetDC(window), format_index, &pfd);

    if (!succeeded)
    {
        OutputDebugStringA("Could not set pixel format\n");
        g_running = 0;
        return;
    }

    HGLRC dummy_context = wglCreateContext(GetDC(window));
    if (!dummy_context)
    {
        OutputDebugStringA("Could not create GL context. Exiting");
        g_running = 0;
        return;
    }
    wglMakeCurrent(GetDC(window), dummy_context);
    if (!succeeded)
    {
        OutputDebugStringA("Could not set current GL context. Exiting");
        g_running = 0;
        return;
    }

    GLenum glew_result = glewInit();
    if (glew_result != GLEW_OK)
    {
        OutputDebugStringA("Could not init glew.\n");
        g_running = 0;
        return;
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
    wglChoosePixelFormatARB(GetDC(window), pixel_attribs, NULL, 10 /*max_formats*/, &format_index, &num_formats);
    if (!num_formats)
    {
        OutputDebugStringA("Could not choose pixel format. Exiting.");
        g_running = 0;
        return;
    }

    succeeded = 0;
    for (uint32_t i = 0; i < num_formats - 1; ++i)
    {
        int local_index = (&format_index)[i];
        succeeded = SetPixelFormat(GetDC(window), local_index, &pfd);
        if (succeeded)
            return;
    }
    if (!succeeded)
    {
        OutputDebugStringA("Could not set pixel format for final rendering context.\n");
        g_running = 0;
        return;
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
}

static void win32_process_input(HWND window)
{
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

    }
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
            win32_setup_context(window, &g_glcontext_handle);
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

    MiltonState milton_state;
    milton_init(&milton_state);

    while (g_running)
    {
        win32_process_input(window);
        glClearColor(0.0f, 1.0f, 0.0f, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        SwapBuffers(GetDC(window));
        // Sleep for a while
        Sleep(10);
    }

    return TRUE;
}
