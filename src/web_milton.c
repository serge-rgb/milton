#include <stdio.h>

#include <SDL2/SDL.h>
//#include <SDL2/SDL_opengl.h>
#include <SDL/SDL_opengl.h>

#include <assert.h>
#include <stdio.h>

#include "libserg/memory.h"
#include "milton.h"

int main()
{
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_GLContext context;

    assert (SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    window = SDL_CreateWindow("Milton", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            960, 480, SDL_WINDOW_OPENGL);

    assert (window);
    context = SDL_GL_CreateContext(window);


    size_t total_memory_size = 256 * 1024 * 1024;
    size_t frame_heap_in_MB  = 32  * 1024 * 1024;

    void* big_chunk_of_memory = malloc(total_memory_size);
    assert(big_chunk_of_memory);
    Arena root_arena = arena_init(big_chunk_of_memory, total_memory_size);
    puts("pre-init");
    Arena transient_arena = arena_spawn(&root_arena, frame_heap_in_MB);
    puts("post-init");

    MiltonState milton_state = {0};
    {
        milton_state.root_arena = &root_arena;
        milton_state.transient_arena = &transient_arena;

        milton_init(&milton_state);
    }


    free(big_chunk_of_memory);

    printf("Done.\n");
    return 0;
}
