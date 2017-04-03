uniform sampler2D u_canvas;
uniform vec2 u_screen_size;
uniform int u_kernel_size;

void
main()
{
#if HAS_TEXTURE_MULTISAMPLE
    #error "Not implemented"
#else
    vec2 screen_point = vec2(gl_FragCoord.x, gl_FragCoord.y);
    vec2 coord = screen_point / u_screen_size;
    vec4 color = texture(u_canvas, coord);
    for ( int y = -u_kernel_size/2; y < u_kernel_size/2; ++y ) {
        for ( int x = -u_kernel_size/2; x < u_kernel_size/2; ++x ) {
            color += texture(u_canvas, (screen_point + vec2(x,y)) / u_screen_size);
        }
    }
    color /= u_kernel_size*u_kernel_size;
#endif
    out_color = color;
}
