// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#if HAS_TEXTURE_MULTISAMPLE
    uniform sampler2DMS u_canvas;
#else
    uniform sampler2D u_canvas;
#endif

uniform vec2      u_screen_size;

void
main()
{
    vec2 coord = gl_FragCoord.xy / u_screen_size;
#if HAS_TEXTURE_MULTISAMPLE
    vec3 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), 0).rgb;
#else
    vec3 color = texture(u_canvas, coord).rgb;
#endif


    out_color = vec4(vec3(1)-color, 1);
}
