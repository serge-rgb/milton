// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "milton.h"
#include "render_common.h"

#if defined(__cplusplus)
extern "C" {
#endif


// Renders canvas and GUI (except dear imgui, which is most of the GUI)
void milton_render(MiltonState* milton_state, /*MiltonRenderFlags*/int render_flags, v2i pan_delta);

void milton_render_to_buffer(MiltonState* milton_state, u8* buffer,
                             i32 x, i32 y,
                             i32 w, i32 h, int scale);

#if defined(__cplusplus)
}
#endif
