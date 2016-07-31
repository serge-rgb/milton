
attribute vec2 a_position;
// attribute vec2 a_uv;

out vec2 v_uv;

void main()
{
    // v_uv = a_uv;
    gl_Position.xy = canvas_to_raster_gl(a_position);
}
