// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


// Per-stroke uniforms
uniform vec4 u_brush_color;

// CanvasView elements:
uniform mat2 u_rotation;
uniform mat2 u_rotation_inverse;
uniform ivec2 u_pan_center;
uniform ivec2 u_zoom_center;
uniform vec2  u_screen_size;
uniform int   u_scale;
uniform int   u_radius;

vec2
canvas_to_raster_gl(vec2 cp)
{
    vec2 xy = (cp - u_pan_center) / u_scale;

    vec2 rp = ( (u_rotation_inverse * xy) + u_zoom_center ) / u_screen_size;

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
    vec2 canvas_point = (u_rotation * (raster_point - u_zoom_center) * u_scale) + vec2(u_pan_center);

    return canvas_point;
}
