// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

uniform sampler2D u_canvas;
in vec2 v_uv;

void
main()
{
    // vec4 color = texture2D(u_canvas, v_uv);
    vec4 color = texture(u_canvas, v_uv);

    out_color = color;
}
