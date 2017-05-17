// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "utils.h"
#include "hardware_renderer.h"  // TODO: Stroke has a RenderElement
                                // member. Refactor to eliminate this
                                // dependency


struct Brush
{
    i32 radius;  // This should be replaced by a BrushType and some union containing brush info.
    v4f color;
    f32 alpha;
};

struct Stroke
{
    i32             id;

    Brush           brush;
    v2l*            points;
    f32*            pressures;
    i32             num_points;
    i32             layer_id;
    Rect            bounding_rect;
    RenderElement   render_element;
};
