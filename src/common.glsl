
// MiltonState elements
uniform vec3 u_background_color;

// Per-stroke uniforms
uniform vec4 u_brush_color;

// CanvasView elements:
uniform ivec2 u_pan_vector;
uniform ivec2 u_screen_center;
uniform vec2  u_screen_size;
uniform int   u_scale;
uniform int   u_radius;


#if GL_core_profile
vec3 as_vec3(ivec3 v)
{
    return vec3(v);
}
vec4 as_vec4(ivec3 v)
{
    return vec4(vec3(v), 1);
}
vec4 as_vec4(vec3 v)
{
    return vec4(v, 1);
}
ivec2 as_ivec2(int v)
{
    return ivec2(v);
}
ivec2 as_ivec2(vec2 v)
{
    return ivec2(v);
}
ivec3 as_ivec3(vec3 v)
{
    return ivec3(v);
}
vec2 as_vec2(ivec2 v)
{
    return vec2(v);
}
#define VEC2 vec2
#define VEC3 vec3
#define VEC4 vec4

#endif

vec2 canvas_to_raster_gl(vec2 cp)
{
    vec2 rp = as_vec2( ((u_pan_vector + as_ivec2(cp)) / as_ivec2(u_scale)) + u_screen_center );
    // rp in [0, W]x[0, H]

    rp /= u_screen_size;
    /* // rp in [0, 1]x[0, 1] */

    rp *= 2.0;
    rp -= 1.0;
    /* // rp in [-1, 1]x[-1, 1] */

    rp.y *= -1.0;

    return vec2(rp);
}

vec2 raster_to_canvas_gl(vec2 raster_point)
{
    vec2 canvas_point = ((raster_point - u_screen_center) * u_scale) - VEC2(u_pan_vector);
    //vec2 canvas_point = ((raster_point - u_screen_center) * u_scale) - u_pan_vector;
    /* { */
    /*     ((raster_point.x - view->screen_center.x) * view->scale) - view->pan_vector.x, */
    /*     ((raster_point.y - view->screen_center.y) * view->scale) - view->pan_vector.y, */
    /* }; */

    return canvas_point;
}

