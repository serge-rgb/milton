uniform sampler2D u_canvas;
uniform vec2      u_screen_size;

void main()
{
    vec2 coord = gl_FragCoord.xy / u_screen_size;
    vec3 color = texture2D(u_canvas, coord).rgb;

    gl_FragColor = vec4(vec3(1)-color, 1);
}
