#include <stdio.h>
#include <SDL2/SDL.h>

#include <assert.h>
#include <stdio.h>


int main()
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_GLContext context;

    assert (SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    window = SDL_CreateWindow("Milton", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            640, 480, SDL_WINDOW_OPENGL);

    int width = 1720;
    int height = 940;
    //SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_OPENGL, &window, &renderer);
    assert (window);
    return 0;
}
