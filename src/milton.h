#pragma once

#include "libserg/serg_io.h"

typedef int32_t bool32;

typedef struct
{
    int32_t     width;
    int32_t     height;
    uint8_t     bytes_per_pixel;
    uint8_t*    raster_buffer;
    // Heap
    Arena*      root_arena;     // Persistent memory.
    Arena*      frame_arena;    // Gets reset after every call to milton_update().
} MiltonState;

typedef struct
{
    int foo;
} MiltonInput;

static void milton_init(MiltonState* milton_state)
{
    // Allocate enough memory for the maximum possible supported resolution. As
    // of now, it seems like future 8k displays will adopt this resolution.
    milton_state->width             = 7680;
    milton_state->height            = 4320;
    milton_state->bytes_per_pixel   = 4;
    int closest_power_of_two = (1 << 27);  // Ceiling of log2(width * height * bpp)
    milton_state->raster_buffer = arena_alloc_array(milton_state->root_arena,
            closest_power_of_two, uint8_t);
}

static void milton_update(MiltonState* milton_state, MiltonInput* input)
{

}
