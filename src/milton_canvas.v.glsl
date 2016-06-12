// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

//#version 120

attribute vec2 a_position;

// CanvasView elements:
uniform ivec2 u_pan_vector;
uniform ivec2 u_screen_center;
uniform vec2 u_screen_size;
uniform int   u_scale;
/* uniform vec2 u_pan_vector; */
/* uniform vec2 u_screen_center; */
/* uniform vec2 u_screen_size; */
/* uniform int  u_scale; */

// C++
#if GL_core_profile
ivec2 as_ivec2(int v)
{
    return ivec2(v);
}
ivec2 as_ivec2(vec2 v)
{
    return ivec2(v);
}
vec2 as_vec2(ivec2 v)
{
    return vec2(v);
}
#endif


vec2 canvas_to_raster_gl(vec2 cp)
{
    vec2 rp = as_vec2( ((u_pan_vector + as_ivec2(cp)) / as_ivec2(u_scale)) + u_screen_center );
    //vec2 rp = vec2( ((u_pan_vector + ivec2(cp)) / ivec2(u_scale)) + u_screen_center );
    //vec2 rp = ((u_pan_vector + cp) / vec2(u_scale)) + u_screen_center;
    // rp in [0, W]x[0, H]

    rp /= u_screen_size;
    // rp in [0, 1]x[0, 1]

    rp *= 2.0;
    rp -= 1.0;
    // rp in [-1, 1]x[-1, 1]

    rp.y *= -1.0;

    return vec2(rp);
}

void main()
{
    gl_Position.xy = canvas_to_raster_gl(a_position);
}

