uniform vec2        u_screen_size;
uniform sampler2D   u_canvas;


void main()
{
    vec4 sample0 = texture2D(u_canvas, vec2(2.0*gl_FragCoord.xy + vec2(0)   - vec2(0.5)) / u_screen_size);
    vec4 sample1 = texture2D(u_canvas, vec2(2.0*gl_FragCoord.xy + vec2(1,0) - vec2(0.5)) / u_screen_size);
    vec4 sample2 = texture2D(u_canvas, vec2(2.0*gl_FragCoord.xy + vec2(0,1) - vec2(0.5)) / u_screen_size);
    vec4 sample3 = texture2D(u_canvas, vec2(2.0*gl_FragCoord.xy + vec2(1,1) - vec2(0.5)) / u_screen_size);

    //gl_FragColor = (sample0) / 1.0;
    gl_FragColor = (sample0 + sample1 + sample2 + sample3) / 4.0;
}
