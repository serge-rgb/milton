// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "utils.h"
#include "renderer.h"  // TODO: Stroke has a RenderElement
                       // member. Refactor to eliminate this
                       // dependency

static const f32 k_max_hardness = 10.0f;


struct Brush
{
    i32 radius;  // In pixels. See milton->brush_sizes for canvas-space sizes.
    v4f color;
    f32 alpha;
    f32 pressure_opacity_min;  // Opacity from pressure.
    f32 hardness;
};


// TODO: These should be brush flags. Probably want to do it when we add a new member to the Brush struct..
enum StrokeFlag
{
    StrokeFlag_PRESSURE_TO_OPACITY  = (1<<0),
    StrokeFlag_DISTANCE_TO_OPACITY  = (1<<1),
    StrokeFlag_ERASER               = (1<<2),
    StrokeFlag_RELATIVE_TO_CANVAS   = (1<<3),
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

    u32 flags; // StrokeFlag

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

struct BrushPreV8
{
    i32 radius;
    v4f color;
    f32 alpha;
    f32 pressure_opacity_min;  // Opacity from pressure.
};

struct BrushPreV7
{
    i32 radius;
    v4f color;
    f32 alpha;
};

static inline Brush default_brush()
{
    Brush brush = {};
    brush.radius = 10240;
    brush.alpha = 1.0f;
    brush.hardness = 10.0f;
    return brush;
}