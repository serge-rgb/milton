#version 150

uniform vec2        u_screen_size;
uniform sampler2D   u_canvas;


void main()
{
    const float ssaa_factor = 4.0;
    vec4 sample0 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(0)   - ivec2(0.5)) / ivec2(u_screen_size), 0);
    vec4 sample1 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(1,0) - ivec2(0.5)) / ivec2(u_screen_size), 0);
    vec4 sample2 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(2,0) - ivec2(0.5)) / ivec2(u_screen_size), 0);
    vec4 sample3 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(3,0) - ivec2(0.5)) / ivec2(u_screen_size), 0);

    vec4 sample4 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(0,1) - ivec2(0.5)) / ivec2(u_screen_size), 0);
    vec4 sample5 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(1,1) - ivec2(0.5)) / ivec2(u_screen_size), 0);
    vec4 sample6 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(2,1) - ivec2(0.5)) / ivec2(u_screen_size), 0);
    vec4 sample7 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(3,1) - ivec2(0.5)) / ivec2(u_screen_size), 0);

    vec4 sample8 = texelFetch(u_canvas, ivec2(ssaa_factor* ivec2(gl_FragCoord.xy) + ivec2(0,2) - ivec2(0.5)) /  ivec2(u_screen_size), 0);
    vec4 sample9 = texelFetch(u_canvas, ivec2(ssaa_factor* ivec2(gl_FragCoord.xy) + ivec2(1,2) - ivec2(0.5)) /  ivec2(u_screen_size), 0);
    vec4 sample10 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(2,2) - ivec2(0.5)) / ivec2(u_screen_size), 0);
    vec4 sample11 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(3,2) - ivec2(0.5)) / ivec2(u_screen_size), 0);

    vec4 sample12 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(0,3) - ivec2(0.5)) / ivec2(u_screen_size), 0);
    vec4 sample13 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(1,3) - ivec2(0.5)) / ivec2(u_screen_size), 0);
    vec4 sample14 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(2,3) - ivec2(0.5)) / ivec2(u_screen_size), 0);
    vec4 sample15 = texelFetch(u_canvas, ivec2(ssaa_factor*ivec2(gl_FragCoord.xy) + ivec2(3,3) - ivec2(0.5)) / ivec2(u_screen_size), 0);

    //gl_FragColor = (sample0) / 1.0;
    gl_FragColor = (sample0 + sample1 + sample2 + sample3 + sample4 + sample5 + sample6 + sample7
                    +sample8 + sample9 + sample10 + sample11 + sample12 + sample12 + sample13 + sample14
                    +sample15) / 16.0;
}
