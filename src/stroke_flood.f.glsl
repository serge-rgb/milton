uniform sampler2D u_canvas;

void main()
{
    vec2 coord = gl_FragCoord.xy / u_screen_size;
    coord.y = 1-coord.y;
    vec4 color = texture2D(u_canvas, coord);
    gl_FragColor = brush_is_eraser() ? vec4(0) : u_brush_color;
    // Visualization
    //gl_FragColor = brush_is_eraser() ? vec4(0) : vec4(1,0,0,0.3);
}
