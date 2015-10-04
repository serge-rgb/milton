
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


#include "define_types.h"

typedef enum {
    Caught_NONE = 0,

    Caught_PRESSURE = (1 << 0),
    Caught_POINT    = (1 << 1),
} NativeEventResult;

typedef struct TabletState_s TabletState;

int milton_main();

void*   platform_allocate(size_t size);
#define platform_deallocate(pointer) platform_deallocate_internal((pointer)); {(pointer) = NULL;}
#define milton_log platform_milton_log
void    milton_fatal(char* message);
void    milton_die_gracefully(char* message);

// Currently only used to poll for tablet data. Called from SDL as a function
// inserted in the main event loop that has access to OS-dependent data and
// API's
NativeEventResult platform_native_event_poll(TabletState* tablet_state, SDL_SysWMEvent event,
                                             i32 width, i32 height,
                                             v2i* out_point,
                                             f32* out_pressure);
void platform_wacom_init(TabletState* tablet_state, SDL_Window* window);
void platform_wacom_deinit(TabletState* tablet_state);


