
// MiltonState elements
uniform vec3 u_background_color;

// Per-stroke uniforms
uniform vec4 u_brush_color;

// CanvasView elements:
uniform ivec2 u_pan_vector;
uniform ivec2 u_screen_center;
uniform vec2  u_screen_size;
uniform int   u_scale;


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
    // rp in [0, 1]x[0, 1]

    rp *= 2.0;
    rp -= 1.0;
    // rp in [-1, 1]x[-1, 1]

    rp.y *= -1.0;

    return vec2(rp);
}

// x,y  - closest point
// z    - t in [0,1] interpolation value
vec3 closest_point_in_segment_gl(vec2 a, vec2 b,
                                 vec2 ab, float ab_magnitude_squared,
                                 vec2 point)
{
    vec3 result;
    float mag_ab = sqrt(ab_magnitude_squared);
    float d_x = ab.x / mag_ab;
    float d_y = ab.y / mag_ab;
    float ax_x = float(point.x - a.x);
    float ax_y = float(point.y - a.y);
    float disc = d_x * ax_x + d_y * ax_y;
    if (disc < 0.0) disc = 0.0;
    if (disc > mag_ab) disc = mag_ab;
    result.z = disc / mag_ab;
    result.xy = VEC2(int(a.x + disc * d_x), int(a.y + disc * d_y));
    return result;
}

