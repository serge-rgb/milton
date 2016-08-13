uniform sampler2D u_canvas;
uniform vec2      u_screen_size;


void main()
{
    vec4 g_eraser_magic = vec4(0,1,0,1);


    //vec2 coord = gl_FragCoord.xy/ u_screen_size;
    vec2 coord = vec2(gl_FragCoord.x, u_screen_size.y-gl_FragCoord.y) / u_screen_size;

    vec4 color = texture2D(u_canvas, coord);
    //vec4 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), 0);

    if (color == g_eraser_magic)
    {
        discard;
    }
    gl_FragColor = vec4(color);
}
