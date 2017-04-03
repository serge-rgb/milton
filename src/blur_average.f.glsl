uniform sampler2D u_canvas;
uniform vec2 u_screen_size;

void
main()
{
#if HAS_TEXTURE_MULTISAMPLE
    #error "Not implemented"
#else
    vec2 screen_point = vec2(gl_FragCoord.x, gl_FragCoord.y);
    vec2 coord = screen_point / u_screen_size;
    vec4 color = texture(u_canvas, coord);
    int kernel_size = 32;
    for ( int y = -kernel_size/2; y < kernel_size/2; ++y ) {
        for ( int x = -kernel_size/2; x < kernel_size/2; ++x ) {
            color += texture(u_canvas, (screen_point + vec2(x,y)) / u_screen_size);
        }
    }
    color /= kernel_size*kernel_size;
#endif
    out_color = color;
}
