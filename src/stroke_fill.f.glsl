// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// in vec3 v_pointa;
// in vec3 v_pointb;

uniform sampler2D u_info;

#ifndef PRESSURE_TO_OPACITY
#define PRESSURE_TO_OPACITY 1
#endif

#ifndef DISTANCE_TO_OPACITY
#define DISTANCE_TO_OPACITY 0
#endif

void
main()
{
    vec2 coord = gl_FragCoord.xy / u_screen_size;
    vec2 stroke_info = texture(u_info, coord).ra;
    float dist = stroke_info.x;
    float pressure = stroke_info.y;
    if ( dist < u_radius*pressure ) {
        out_color = u_brush_color;
        #if PRESSURE_TO_OPACITY
            out_color.a *= pressure;
        #endif
        #if DISTANCE_TO_OPACITY
            out_color.a *= 1 - dist / (u_radius*pressure);
        #endif
    }
    else {
        discard;
    }
}
