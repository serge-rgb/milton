
attribute vec2 a_point;
attribute vec2 a_uv;
varying vec2 v_uv;

void main()
{
    v_uv = a_uv;
    gl_Position = vec4(a_point, 0,1);
}
