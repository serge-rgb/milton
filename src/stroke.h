// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "utils.h"
#include "renderer.h"  // TODO: Stroke has a RenderElement
                       // member. Refactor to eliminate this
                       // dependency


struct Brush
{
    i32 radius;
    v4f color;
    f32 alpha;
    f32 pressure_opacity_min;  // Opacity from pressure.
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
    RenderHandle    render_handle;

    enum
    {
        StrokeFlag_PRESSURE_TO_OPACITY = (1<<0),
    } flags;

#if STROKE_DEBUG_VIZ
    enum DebugFlags
    {
      NONE = (0),
      INTERPOLATED  = (1<<0),
    };
    int* debug_flags;
#endif
};


// ==== Old versions ====

struct BrushPreV7
{
    i32 radius;
    v4f color;
    f32 alpha;
};
