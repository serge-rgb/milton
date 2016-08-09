uniform vec2        u_screen_size;
uniform sampler2D   u_canvas;


void main()
{
    const float ssaa_factor = 4.0;
    vec4 sample0 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(0)   - ivec2(0.5)) / u_screen_size);
    vec4 sample1 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(1,0) - ivec2(0.5)) / u_screen_size);
    vec4 sample2 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(2,0) - ivec2(0.5)) / u_screen_size);
    vec4 sample3 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(3,0) - ivec2(0.5)) / u_screen_size);

    vec4 sample4 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(0,1) - ivec2(0.5)) / u_screen_size);
    vec4 sample5 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(1,1) - ivec2(0.5)) / u_screen_size);
    vec4 sample6 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(2,1) - ivec2(0.5)) / u_screen_size);
    vec4 sample7 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(3,1) - ivec2(0.5)) / u_screen_size);

    vec4 sample8 = texture2D(u_canvas, vec2(ssaa_factor* gl_FragCoord.xy + ivec2(0,2) - ivec2(0.5)) / u_screen_size);
    vec4 sample9 = texture2D(u_canvas, vec2(ssaa_factor* gl_FragCoord.xy + ivec2(1,2) - ivec2(0.5)) / u_screen_size);
    vec4 sample10 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(2,2) - ivec2(0.5)) / u_screen_size);
    vec4 sample11 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(3,2) - ivec2(0.5)) / u_screen_size);

    vec4 sample12 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(0,3) - ivec2(0.5)) / u_screen_size);
    vec4 sample13 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(1,3) - ivec2(0.5)) / u_screen_size);
    vec4 sample14 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(2,3) - ivec2(0.5)) / u_screen_size);
    vec4 sample15 = texture2D(u_canvas, vec2(ssaa_factor*gl_FragCoord.xy + ivec2(3,3) - ivec2(0.5)) / u_screen_size);

    //gl_FragColor = (sample0) / 1.0;
    gl_FragColor = (sample0 + sample1 + sample2 + sample3 + sample4 + sample5 + sample6 + sample7
                    +sample8 + sample9 + sample10 + sample11 + sample12 + sample12 + sample13 + sample14
                    +sample15) / 16.0;
}
