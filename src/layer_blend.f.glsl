#version 150

uniform sampler2DMS u_canvas;


in vec2 v_uv;

void main()
{
    vec4 g_eraser_magic = vec4(0,1,0,1);

    vec4 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), 0);

    if (color == g_eraser_magic)
    {
        discard;
    }
    gl_FragColor = vec4(color);
}
