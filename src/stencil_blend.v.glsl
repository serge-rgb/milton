
attribute vec2 a_position;
attribute vec2 a_uv;

void main()
{
    gl_Position.xy = canvas_to_raster_gl(a_position);
}
