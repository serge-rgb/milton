// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.


#pragma once

#include "common.h"

#define RENDER_STACK_SIZE       (1 << 20)

// Render Workers:
//    We have a bunch of workers running on threads, who wait on a lockless
//    queue to take BlockgroupRenderData structures.
//    When there is work available, they call blockgroup_render_thread with the
//    appropriate parameters.
typedef struct {
    i32     block_start;
} BlockgroupRenderData;

typedef struct {
    Rect*   blocks;  // Screen areas to render.
    i32     num_blocks;
    u32*    canvas_buffer;

    // LIFO work queue
    SDL_mutex*              mutex;
    BlockgroupRenderData    blockgroup_render_data[RENDER_STACK_SIZE];
    i32                     index;

    SDL_sem*   work_available;
    SDL_sem*   completed_semaphore;
} RenderStack;


typedef enum {
    MiltonRenderFlags_NONE              = 0,

    MiltonRenderFlags_PICKER_UPDATED    = 1 << 0,
    MiltonRenderFlags_FULL_REDRAW       = 1 << 1,
    MiltonRenderFlags_FINISHED_STROKE   = 1 << 2,
    MiltonRenderFlags_PAN_COPY          = 1 << 3,
    MiltonRenderFlags_BRUSH_PREVIEW     = 1 << 4,
    MiltonRenderFlags_BRUSH_HOVER       = 1 << 5,
    MiltonRenderFlags_DRAW_ITERATIVELY  = 1 << 6,
} MiltonRenderFlags;
