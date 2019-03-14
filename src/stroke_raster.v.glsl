// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

in vec3 a_position;
in vec3 a_pointa;
in vec3 a_pointb;

out vec3 v_pointa;
out vec3 v_pointb;

#if STROKE_DEBUG_VIZ
in vec3 a_debug_color;
out vec3 v_debug_color;
#endif

#define MAX_DEPTH_VALUE 1048576.0


void
main()
{
    v_pointa = a_pointa;
    v_pointb = a_pointb;

#if STROKE_DEBUG_VIZ
    v_debug_color = a_debug_color;
#endif
    gl_Position.xy = canvas_to_raster_gl(a_position.xy);
    gl_Position.w = 1;

    gl_Position.z = a_position.z / MAX_DEPTH_VALUE;
}

