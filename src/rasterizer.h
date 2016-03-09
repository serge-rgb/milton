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
