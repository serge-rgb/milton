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


#include "SDL.h"

#define MILTON_DESKTOP
#include "system_includes.h"

int milton_main();

#if defined(_WIN32)
#include "platform_windows.h"
#elif defined(__linux__)
#endif

#include "libnuwen/memory.h"

#include "milton.h"

int milton_main()
{
    // Note: Possible crash regarind SDL_main entry point.
    // Note: Event handling, File I/O and Threading are initialized by default
    SDL_Init(SDL_INIT_VIDEO);

    i32 width = 1280;
    i32 height = 800;
    SDL_Window* window = SDL_CreateWindow("Milton",
                                 SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        puts("[ERROR] -- Exiting. SDL could not create window");
        exit(EXIT_FAILURE);
    }
    SDL_Delay(3000);
    SDL_Quit();

    return 0;
}
