#version 150

attribute vec2 a_position;
attribute vec2 a_norm;  // Normalized position

out vec2 v_norm;

void main()
{
    v_norm = a_norm;
    gl_Position = vec4(a_position, 0, 1);
}
