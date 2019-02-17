// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

in vec3 a_position;
in vec3 a_pointa;
in vec3 a_pointb;
in vec2 a_pointp;
in vec2 a_pointq;

out vec3 v_pointa;
out vec3 v_pointb;
out vec2 v_pointp;
out vec2 v_pointq;

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
    v_pointp = a_pointp;
    v_pointq = a_pointq;

#if STROKE_DEBUG_VIZ
    v_debug_color = a_debug_color;
#endif
    gl_Position.xy = canvas_to_raster_gl(a_position.xy);
    gl_Position.w = 1;

    gl_Position.z = a_position.z / MAX_DEPTH_VALUE;
}

