// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

uniform sampler2D u_canvas;
uniform vec2 u_screen_size;
uniform int u_kernel_size;
uniform int u_direction;


void
main()
{
    vec2 screen_point = vec2(gl_FragCoord.x, gl_FragCoord.y);
    // LINEAR
    vec2 half_pixel_size = 1 / (2*u_screen_size);
    int n_samples = 0;
    out_color = vec4(0);
    if ( u_kernel_size > 1 ) {
        if ( u_direction == 0 ) {
            for ( int y = -u_kernel_size+1; y < u_kernel_size; y+=2 ) {
                vec2 coord = (screen_point+vec2(0.0,y-0.5)) / u_screen_size;
                vec4 sample = texture(u_canvas, coord);
                out_color += sample * sample;
            }
        } else {
            for ( int x = -u_kernel_size+1; x < u_kernel_size; x+=2 ) {
                vec2 coord = (screen_point+vec2(x-0.5,0.0)) / u_screen_size;
                vec4 sample = texture(u_canvas, coord);
                out_color += sample * sample;
            }
        }
        out_color /= u_kernel_size;
        out_color = sqrt(out_color);
    } else {
        out_color = texture(u_canvas, screen_point / u_screen_size);
    }
}
