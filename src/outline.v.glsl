
attribute vec2 a_position;
attribute vec2 a_sizes;

varying vec2 v_sizes;

void main()
{
    v_sizes = a_sizes;
    gl_Position.xy = a_position;
}
