// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "milton.h"


#if defined(__cplusplus)
extern "C" {
#endif


typedef struct WorkerParams
{
    MiltonState* milton_state;
    i32 worker_id;
} WorkerParams;

// Declared here so that the workers get launched from the init function.
int renderer_worker_thread(/* WorkerParams* */void* data);

// Blocking function. When it returns, the framebuffer is updated to the
// current state. It does a lot of smart things to do as little work as
// possible. Most users on most machines should get interactive framerates.
void milton_render(MiltonState* milton_state, MiltonRenderFlags render_flags, v2i pan_delta);

void milton_render_to_buffer(MiltonState* milton_state, u8* buffer,
                             i32 x, i32 y,
                             i32 w, i32 h, int scale);

#if defined(__cplusplus)
}
#endif
