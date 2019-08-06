// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

uniform float u_opacity_min;
uniform float u_hardness;
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
    float pressure = stroke_info.y;
    if ( stroke_info.x < 1.0f  ) {
        out_color = u_brush_color;
        #if PRESSURE_TO_OPACITY
            out_color *= (1.0f - u_opacity_min) * pressure + u_opacity_min;
        #endif
        #if DISTANCE_TO_OPACITY
            out_color *= pow(1 - (stroke_info.x), 1.0f / u_hardness);
        #endif
    }
    else {
        discard;
    }
}
