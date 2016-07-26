#version 150

uniform sampler2D u_canvas;


in vec2 v_uv;

void main()
{
    vec4 g_eraser_magic = vec4(0,1,0,1);

    vec4 color = texture2D(u_canvas, v_uv);

    if (color == g_eraser_magic)
    {
        discard;
    }
    gl_FragColor = vec4(color);
}
