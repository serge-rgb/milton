// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


// Per-stroke uniforms
uniform vec4 u_brush_color;

// CanvasView elements:
uniform ivec2 u_pan_center;
uniform ivec2 u_zoom_center;
uniform vec2  u_screen_size;
uniform int   u_scale;
uniform int   u_radius;

vec2
canvas_to_raster_gl(vec2 cp)
{
    vec2 rp = ( ((cp - u_pan_center) / u_scale) + u_zoom_center ) / u_screen_size;

    // rp in [0, 1]x[0, 1]

    rp *= 2.0;
    rp -= 1.0;
    /* // rp in [-1, 1]x[-1, 1] */

    rp.y *= -1.0;

    return vec2(rp);
}

vec2
raster_to_canvas_gl(vec2 raster_point)
{
    vec2 canvas_point = ((raster_point - u_zoom_center) * u_scale) + vec2(u_pan_center);

    return canvas_point;
}

bool
brush_is_eraser()
{
    bool is_eraser = false;
    // Constant k_eraser_color defined in canvas.cc
    if ( u_brush_color == vec4(23,34,45,56) ) {
        is_eraser = true;
    }
    return is_eraser;
}


vec4
blend(vec4 dst, vec4 src)
{
    vec4 result = src + dst*(1.0f-src.a);

    return result;
}
