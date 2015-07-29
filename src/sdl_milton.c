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
    // TODO: Specify OpenGL 3.0

    // Note: Possible crash regarind SDL_main entry point.
    // Note: Event handling, File I/O and Threading are initialized by default
    SDL_Init(SDL_INIT_VIDEO);

    i32 width = 1280;
    i32 height = 800;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetSwapInterval(0);

    SDL_Window* window = SDL_CreateWindow("Milton",
                                 SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 width, height,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        milton_log("[ERROR] -- Exiting. SDL could not create window\n");
        exit(EXIT_FAILURE);
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if (!gl_context)
    {
        milton_log("Could not create OpenGL context\n");
    }

    GLenum glew_err = glewInit();

    if (glew_err != GLEW_OK)
    {
        milton_log("glewInit failed with error: %s\nExiting.\n",
                   glewGetErrorString(glew_err));
        exit(EXIT_FAILURE);
    }

    // ==== Intialize milton
    //  Total memory requirement for Milton
    size_t total_memory_size = (size_t)2 * 1024 * 1024 * 1024;
    //  Size of frame heap
    size_t frame_heap_in_MB  = 32 * 1024 * 1024;

    void* big_chunk_of_memory = allocate_big_chunk_of_memory(total_memory_size);

    assert (big_chunk_of_memory);

    Arena root_arena      = arena_init(big_chunk_of_memory, total_memory_size);
    Arena transient_arena = arena_spawn(&root_arena, frame_heap_in_MB);

    MiltonState* milton_state = arena_alloc_elem(&root_arena, MiltonState);
    {
        milton_state->root_arena = &root_arena;
        milton_state->transient_arena = &transient_arena;

        milton_init(milton_state, 2560, 1600);
    }


    b32 should_quit = false;
    MiltonInput milton_input = { 0 };
    milton_input.flags |= MiltonInputFlags_FULL_REFRESH;  // Full draw on first launch
    milton_resize(milton_state, (v2i){0}, (v2i){width, height});

    while(!should_quit)
    {
        // ==== Handle events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                {
                    should_quit = true;
                    break;
                }
                // TODO: Handle
                //  - resizing
                //  - (MiltonInput) point
                //  - (MiltonInput) pan_delta
                //  - (MiltonInput) scale
                //  - MiltonFlags
                //      - Full refresh
                //      - reset
                //      - end stroke
                //      - undo
                //      - redo
                //      - set-eraser
                //      - set-brush
                //      - fast
            }
            if (should_quit)
            {
                break;
            }

        }
        // ==== Update and render
        milton_update(milton_state, &milton_input);
        milton_input = (MiltonInput){0};
        milton_gl_backend_draw(milton_state);
        SDL_GL_SwapWindow(window);

    }

    SDL_Quit();

    return 0;
}
