// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "common.h"

#define RENDER_STACK_SIZE       (1 << 20)

// Render Workers:
//    We have a bunch of workers running on threads, who wait on a lockless
//    queue to take BlockgroupRenderBackend structures.
//    When there is work available, they call blockgroup_render_thread with the
//    appropriate parameters.
struct BlockgroupRenderBackend
{
    i32     block_start;
};

struct RenderStack
{
    Rect*   blocks;  // Screen areas to render.
    i32     num_blocks;
    u32*    canvas_buffer;

    // LIFO work queue
    SDL_mutex*              mutex;
    BlockgroupRenderBackend    blockgroup_render_data[RENDER_STACK_SIZE];
    i32                     index;

    SDL_sem*   work_available;
    SDL_sem*   completed_semaphore;
};


enum MiltonRenderFlags
{
    MiltonRenderFlags_NONE              = 0,

    MiltonRenderFlags_UI_UPDATED       = 1 << 0,
    MiltonRenderFlags_FULL_REDRAW      = 1 << 1,
    MiltonRenderFlags_FINISHED_STROKE  = 1 << 2,
    MiltonRenderFlags_PAN_COPY         = 1 << 3,
    MiltonRenderFlags_BRUSH_PREVIEW    = 1 << 4,
    MiltonRenderFlags_BRUSH_HOVER      = 1 << 5,
    MiltonRenderFlags_DRAW_ITERATIVELY = 1 << 6,
    MiltonRenderFlags_BRUSH_CHANGE     = 1 << 7,
};
