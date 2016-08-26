#extension GL_ARB_texture_multisample : enable
#extension GL_ARB_sample_shading : enable

uniform sampler2DMS u_canvas;
uniform vec2      u_screen_size;

void main()
{
    vec2 coord = gl_FragCoord.xy / u_screen_size;
    //vec3 color = texture2D(u_canvas, coord).rgb;
    vec3 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), 0).rgb;

    gl_FragColor = vec4(vec3(1)-color, 1);
}
