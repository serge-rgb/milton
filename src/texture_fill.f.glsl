// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


uniform float u_alpha;

uniform sampler2D u_canvas;
uniform vec2 u_screen_size;

void
main()
{
    vec2 screen_point = vec2(gl_FragCoord.x, gl_FragCoord.y);
    vec2 coord = screen_point / u_screen_size;
    vec4 color = texture(u_canvas, coord);
    out_color = color;
    out_color *= u_alpha;
}
