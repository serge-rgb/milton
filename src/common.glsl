
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


// vec3 as_vec3(ivec3 v)
// {
//     return vec3(v);
// }
// vec4 as_vec4(ivec3 v)
// {
//     return vec4(vec3(v), 1);
// }
// vec4 as_vec4(vec3 v)
// {
//     return vec4(v, 1);
// }
// ivec2 as_ivec2(int v)
// {
//     return ivec2(v);
// }
// ivec2 as_ivec2(vec2 v)
// {
//     return ivec2(v);
// }
// ivec3 as_ivec3(vec3 v)
// {
//     return ivec3(v);
// }
// vec2 as_vec2(ivec2 v)
// {
//     return vec2(v);
// }
// #define VEC2 vec2
// #define VEC3 vec3
// #define VEC4 vec4

vec2 canvas_to_raster_gl(vec2 cp)
{
    vec2 rp = ( ((u_pan_vector + cp) / u_scale) + u_screen_center ) / u_screen_size;
    // rp in [0, W]x[0, H]

    //rp /= u_screen_size;
    /* // rp in [0, 1]x[0, 1] */

    rp *= 2.0;
    rp -= 1.0;
    /* // rp in [-1, 1]x[-1, 1] */

    rp.y *= -1.0;

    return vec2(rp);
}

vec2 raster_to_canvas_gl(vec2 raster_point)
{
    vec2 canvas_point = ((raster_point - u_screen_center) * u_scale) - vec2(u_pan_vector);

    return canvas_point;
}

bool brush_is_eraser()
{
    bool is_eraser = false;
    // Constant k_eraser_color defined in canvas.cc
    if ( u_brush_color == vec4(23,34,45,56) ) {
        is_eraser = true;
    }
    return is_eraser;
}


vec4 blend(vec4 dst, vec4 src)
{
    vec4 result = src + dst*(1.0f-src.a);

    return result;
}
