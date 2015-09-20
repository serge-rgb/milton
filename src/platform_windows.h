//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


// The returns value mean different things, but other than that, we're ok
#ifdef _MSC_VER
#define snprintf sprintf_s
#endif

#define HEAP_BEGIN_ADDRESS NULL

#define platform_allocate(total_memory_size) VirtualAlloc(HEAP_BEGIN_ADDRESS, \
                                                          (total_memory_size),\
                                                          MEM_COMMIT | MEM_RESERVE, \
                                                          PAGE_READWRITE)

#define platform_deallocate(pointer) VirtualFree((pointer), 0, MEM_RELEASE); { (pointer) = 0; }

func void milton_fatal(char* message);
void win32_log(char *format, ...);
#define milton_log win32_log

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

func void milton_fatal(char* message)
{
    milton_log("*** [FATAL] ***: \n\t");
    puts(message);
    exit(EXIT_FAILURE);
}

// TODO: Show a message box, and then die
func void milton_die_gracefully(char* message)
{
    milton_log("*** [FATAL] ***: \n\t");
    puts(message);
    exit(EXIT_FAILURE);
}

void platform_load_gl_func_pointers()
{
    GLenum glew_err = glewInit();

    if (glew_err != GLEW_OK)
    {
        milton_log("glewInit failed with error: %s\nExiting.\n",
                   glewGetErrorString(glew_err));
        exit(EXIT_FAILURE);
    }

    if (GLEW_VERSION_1_4)
    {
        if (glewIsSupported("GL_ARB_shader_objects "
                            "GL_ARB_vertex_program "
                            "GL_ARB_fragment_program "
                            "GL_ARB_vertex_buffer_object "))
        {
            milton_log("[DEBUG] GL OK.\n");
        }
        else
        {
            milton_die_gracefully("One or more OpenGL extensions are not supported.\n");
        }
    }
    else
    {
        milton_die_gracefully("OpenGL 1.4 not supported.\n");
    }
    // Load extensions
}

#include "win32_wacom_defines.h"

#define WIN_MAX_WACOM_PACKETS 1024

struct TabletState_s
{
    HWND window;
    BITMAPINFO bitmap_info;

    HINSTANCE wintab_handle;
    WacomAPI wacom;
    UINT wacom_prs_max;
    HCTX wacom_ctx;

    PACKET packet_buffer[WIN_MAX_WACOM_PACKETS];
};

#include "win32_wacom.h"

void platform_wacom_init(TabletState* tablet_state, SDL_Window* window)
{
    // Ask SDL for windows events so we can receive packets
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    SDL_SysWMinfo wminfo;
    SDL_bool wminfo_ok = SDL_GetWindowWMInfo(window, &wminfo);
    if (!wminfo_ok)
    {
        milton_fatal("Could not get WM info from SDL");
    }
    tablet_state->window = wminfo.info.win.window;

    if (!win32_wacom_load_wintab(tablet_state))
    {
        OutputDebugStringA("No wintab.\n");
    }

    win32_wacom_get_context(tablet_state);
}

func NativeEventResult platform_native_event_poll(TabletState* tablet_state, SDL_SysWMEvent event,
                                                  i32 width, i32 height,
                                                  v2i* out_point,
                                                  f32* out_pressure)
{
    NativeEventResult caught_event = Caught_NONE;
    i32 pressure_range = tablet_state->wacom_prs_max;
    if (event.type == SDL_SYSWMEVENT)
    {
        if (event.msg)
        {
            SDL_SysWMmsg msg = *event.msg;
            if (msg.subsystem == SDL_SYSWM_WINDOWS)
            {
                HWND window   = msg.msg.win.hwnd;
                UINT message  = msg.msg.win.msg;
                WPARAM wparam = msg.msg.win.wParam;
                LPARAM lparam = msg.msg.win.lParam;
                switch (message)
                {
                case WT_PACKET:
                    {
                        PACKET pkt = { 0 };
                        HCTX ctx = (HCTX)lparam;
                        if (ctx == tablet_state->wacom_ctx &&
                            tablet_state->wacom.WTPacket(ctx, (UINT)wparam, &pkt))
                        {
                            POINT screen_point =
                            {
                                .x = pkt.pkX,
                                .y = pkt.pkY,
                            };
                            POINT client_point = screen_point;
                            ScreenToClient(tablet_state->window, &client_point);


                            if (client_point.x >= 0 && client_point.x < width &&
                                client_point.y >= 0 && client_point.y < height)
                            {
#if 0
                                milton_log ("Wacom packet X: %d, Y: %d, P: %d\n",
                                            pkt.pkX, pkt.pkY, pkt.pkNormalPressure);
#endif
                                *out_pressure = (f32)pkt.pkNormalPressure / (f32)pressure_range;
                                *out_point = (v2i){client_point.x, client_point.y};
                                caught_event |= Caught_PRESSURE;
                                caught_event |= Caught_POINT;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    return caught_event;
}

void platform_wacom_deinit(TabletState* tablet_state)
{
    WacomAPI* wacom = &tablet_state->wacom;
    wacom->WTClose(tablet_state->wacom_ctx);
}

int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
{
    milton_main();
}
