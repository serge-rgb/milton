// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// TODO: this layout qualifier introduces GLSL 150 dependency.
layout(origin_upper_left) in vec4 gl_FragCoord;

flat in vec3 v_pointa;
flat in vec3 v_pointb;
uniform sampler2D u_canvas;

vec4 blend(vec4 dst, vec4 src)
{
    vec4 result = src + dst*(1.0f-src.a);

    return result;
}

void main()
{
    vec2 coord = gl_FragCoord.xy / u_screen_size;
    coord.y = 1-coord.y;
    vec4 color = texture2D(u_canvas, coord);

    gl_FragColor = blend(color, u_brush_color);
}
