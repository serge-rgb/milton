
uniform sampler2D u_canvas;
in vec2 v_uv;
#if __VERSION__ > 120
out vec4 out_color;
#endif

void
main()
{
    // vec4 color = texture2D(u_canvas, v_uv);
    vec4 color = texture(u_canvas, v_uv);

    out_color = color;
}
