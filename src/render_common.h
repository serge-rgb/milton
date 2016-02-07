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
