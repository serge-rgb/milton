uniform sampler2D u_canvas;

void main()
{
    vec2 coord = vec2(gl_FragCoord.x, u_screen_size.y-gl_FragCoord.y) / u_screen_size;
    coord.y = 1-coord.y;
    vec4 color = texture2D(u_canvas, coord);

    gl_FragColor = brush_is_eraser() ? color : u_brush_color;
    // Visualization
    //gl_FragColor = brush_is_eraser() ? vec4(0) : vec4(1,0,0,0.3);
}
