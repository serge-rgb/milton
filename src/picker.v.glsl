#version 150

attribute vec2 a_position;

void main()
{
    // W00t
    gl_Position = vec4(a_position, 0, 1);
}
