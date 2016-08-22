uniform sampler2D u_canvas;
uniform vec2 u_screen_size;

void main()
{
    vec2 screen_point = vec2(gl_FragCoord.x, u_screen_size.y-gl_FragCoord.y);
    vec2 coord = screen_point / u_screen_size;
    coord.y = 1-coord.y;
    vec4 color = texture2D(u_canvas, coord);
    gl_FragColor = color;
}
