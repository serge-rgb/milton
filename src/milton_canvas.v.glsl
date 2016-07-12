// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Note:
// TODO: The correct thing to do here would be to use ivec2/ivec3, but what
// would be the compat and performance consequences?
attribute vec2 a_position;

// (x,y, pressure)
attribute vec3 a_pointa;
attribute vec3 a_pointb;

varying float v_pressure;
flat out ivec3 v_pointa;
flat out ivec3 v_pointb;

void main()
{
    v_pointa = as_ivec3(a_pointa);
    v_pointb = as_ivec3(a_pointb);
    gl_Position.xy = canvas_to_raster_gl(a_position);
}

