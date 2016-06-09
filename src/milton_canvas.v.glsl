// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#version 120

attribute vec2 position;

// TODO: Switch to floating point math once it is correct.. Will (probably?) be faster.

// CanvasView elements:
uniform ivec2 pan_vector;
uniform ivec2 screen_center;
uniform ivec2 screen_size;
uniform int  scale;

ivec2 canvas_to_raster_gl(ivec2 cp)
{
    ivec2 rp = ((pan_vector + cp) / ivec2(scale)) + screen_center;
    // rp in [0, W]x[0, H]

    rp /= screen_size;
    // rp in [0, 1]x[0, 1]

    rp *= 2;
    rp -= 1;
    // rp in [-1, 1]x[-1, 1]

    return rp;
}

void main()
{
    gl_Position.xy = position;
}

