#extension GL_ARB_texture_multisample : enable
#extension GL_ARB_sample_shading : enable

uniform sampler2DMS u_canvas;
uniform vec2 u_screen_size;

void main()
{
    vec4 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), gl_SampleID);

    gl_FragColor = color;
}
