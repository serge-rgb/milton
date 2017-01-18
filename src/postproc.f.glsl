uniform sampler2D u_canvas;
uniform sampler2D u_depth;
uniform vec2 u_screen_size;

void
main()
{
    // Pixel width / height.
    float pw = 1 / u_screen_size.x;
    float ph = 1 / u_screen_size.y;
    vec2 coord = gl_FragCoord.xy / u_screen_size;

    vec4 c = texture(u_depth, coord, 0);
    vec4 se = texture(u_depth, coord + vec2(-pw, -ph), 0);
    vec4 ne = texture(u_depth, coord + vec2(-pw, +ph), 0);
    vec4 sw = texture(u_depth, coord + vec2(+pw, -ph), 0);
    vec4 nw = texture(u_depth, coord + vec2(+pw, +ph), 0);

    out_color = texture(u_canvas, coord, 0);
    int n = 1;
    if ( c != se ) {
        out_color += texture(u_canvas, coord + vec2(-pw, -ph), 0);
        ++n;
    }
    if (c != ne ) {
        out_color += texture(u_canvas, coord + vec2(-pw, +ph), 0);
        ++n;
    }
    if (c != sw ) {
        out_color += texture(u_canvas, coord + vec2(+pw, -ph), 0);
        ++n;
    }
    if (c != nw ) {
        out_color += texture(u_canvas, coord + vec2(+pw, +ph), 0);
        ++n;
    }
    out_color /= n;
    #if 0
    #endif
}