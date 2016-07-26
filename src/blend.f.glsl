// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// TODO: this layout qualifier introduces GLSL 150 dependency.
layout(origin_upper_left) in vec4 gl_FragCoord;

flat in vec3 v_pointa;
flat in vec3 v_pointb;
uniform sampler2DMS u_canvas;

bool brush_is_eraser()
{
    bool is_eraser = false;
    if (u_brush_color == vec4(23,34,45,56)) // defined in canvas.cc k_eraser_color
    {
        is_eraser = true;
    }
    return is_eraser;
}


vec4 blend(vec4 dst, vec4 src)
{
    vec4 result = src + dst*(1.0f-src.a);

    return result;
}

void main()
{
    vec4 g_eraser_magic = vec4(0,1,0,1);

    vec2 coord = gl_FragCoord.xy / u_screen_size;
    coord.y = 1-coord.y;
    //vec4 color = texture2D(u_canvas, coord);
    vec4 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), 0);

    //if (color.a == 1) { discard; }

    gl_FragColor = brush_is_eraser() ? g_eraser_magic : blend(color, u_brush_color);
    // If rendering front-to-back, with screen cleared:
    //gl_FragColor = brush_is_eraser() ? blend(vec4(u_background_color, 1), color) : blend(u_brush_color, color);
}
