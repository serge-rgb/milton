// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

uniform sampler2D u_canvas;

uniform vec2      u_screen_size;

void
main()
{
    vec2 coord = gl_FragCoord.xy / u_screen_size;
    vec3 color = texture(u_canvas, coord).rgb;

    out_color = vec4(vec3(1)-color, 1);
}
