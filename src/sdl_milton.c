#include <stdlib.h>
#include <stdio.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include "SDL.h"

#include "libserg/memory.h"

#define milton_log printf

#include "milton.h"

int main()
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
}
