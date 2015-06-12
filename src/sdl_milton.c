#include <stdio.h>

#include "SDL.h"

int main()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    puts("Milton.");
    SDL_Quit();
}
