// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

//#version 120

attribute vec2 a_position;

// TODO: Switch to floating point math once it is correct.. Will (probably?) be faster.

// CanvasView elements:
uniform vec2 u_pan_vector;
uniform vec2 u_screen_center;
uniform vec2 u_screen_size;
uniform int  u_scale;

vec2 canvas_to_raster_gl(vec2 cp)
{
    vec2 rp = ((u_pan_vector + cp) / vec2(u_scale)) + u_screen_center;
    // rp in [0, W]x[0, H]

    rp /= u_screen_size;
    // rp in [0, 1]x[0, 1]

    rp *= 2;
    rp -= 1;
    // rp in [-1, 1]x[-1, 1]

    return rp;
}

void main()
{
    gl_Position.xy = a_position;
}

