uniform sampler2D u_canvas;
uniform vec2 u_screen_size;
uniform int u_kernel_size;
uniform int u_direction;


void
main()
{
#if HAS_TEXTURE_MULTISAMPLE
    #error "Not implemented"
#else
    vec2 screen_point = vec2(gl_FragCoord.x, gl_FragCoord.y);
    // LINEAR
    vec2 half_pixel_size = 1 / (2*u_screen_size);
    int n_samples = 0;
    out_color = vec4(0);
    if ( u_kernel_size > 1 ) {
        if ( u_direction == 0 ) {
            for ( int y = -u_kernel_size+1; y < u_kernel_size; y+=2 ) {
                out_color += texture(u_canvas, (screen_point+vec2(0.0,y-0.5)) / u_screen_size);
            }
        } else {
            for ( int x = -u_kernel_size+1; x < u_kernel_size; x+=2 ) {
                out_color += texture(u_canvas, (screen_point+vec2(x-0.5,0.0)) / u_screen_size);
            }
        }
        out_color /= u_kernel_size;
    } else {
        out_color = texture(u_canvas, screen_point / u_screen_size);
    }
#endif  // HAS_TEXTURE_MULTISAMPLE
}
