// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Note:
//  The correct thing to do here would be to use ivec2/ivec3, but what would be the performance consequences?
attribute vec2 a_position;
attribute vec3 a_pointa;
attribute vec3 a_pointb;

varying float v_pressure;
flat out ivec3 v_pointa;
flat out ivec3 v_pointb;

void main()
{
    v_pointa = as_ivec3(a_pointa);
    gl_Position.xy = canvas_to_raster_gl(a_position);
}

