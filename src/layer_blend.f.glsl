#version 150

uniform sampler2D u_canvas;
uniform vec2      u_screen_size;


in vec2 v_uv;

void main()
{
    vec4 g_eraser_magic = vec4(0,1,0,1);


    //vec2 coord = (2.0*vec2(gl_FragCoord.xy) + 1.0) / (2.0*u_screen_size);
    vec2 coord = gl_FragCoord.xy/ u_screen_size;

    vec4 color = texture2D(u_canvas, coord);

    if (color == g_eraser_magic)
    {
        discard;
    }
    gl_FragColor = vec4(color);
}
