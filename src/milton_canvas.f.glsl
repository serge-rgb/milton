// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// TODO: this layout qualifier introduces GLSL 150 dependency.
layout(origin_upper_left) in vec4 gl_FragCoord;

#define PRESSURE_RESOLUTION_GL (1<<20)

flat in ivec3 v_pointa;
flat in ivec3 v_pointb;

//  TODO: Uniform buffer objets are our only relatively non-hairy choice.  Any
//  GPU will run out of memory with a big enough drawing when allocating a
//  fixed-size UBO per-stroke, so there is going to be a change. One
//  possibility is to render drawings in multiple passes, storing a buffer for
//  the painting for a certain number of strokes.  This makes sense since we
//  avoid duplicating rendering work for the most common case which is not
//  panning and not zooming, just drawing over the existing painting.
#define STROKE_MAX_POINTS_GL 512
#if GL_core_profile

// TODO: instead of std140, get offsets with glGetUniformIndices and glGetActiveUNiformsiv GL_UNIFORM_OFFSET
layout(std140) uniform StrokeUniformBlock
{
    int count;
    ivec3 points[STROKE_MAX_POINTS_GL];  // 16KB
} u_stroke;
#endif


vec4 blend(vec4 dst, vec4 src)
{
    vec4 result = src + dst*(1.0f-src.a);

    return result;
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
    if (disc < 0.0)
    {
        disc = 0.0;
    }
    else if (disc > mag_ab)
    {
        disc = mag_ab;
    }
    result.z = disc / mag_ab;
    result.xy = VEC2(int(a.x + disc * d_x), int(a.y + disc * d_y));
    return result;
}

#define MAX_DIST (1<<24)

void main()
{
    vec4 color = u_brush_color;

#if 0
    {
        vec2 ab = VEC2(v_pointb.xy - v_pointa.xy);
        float ab_magnitude_squared = ab.x*ab.x + ab.y*ab.y;

        vec2 fragment_point = raster_to_canvas_gl(gl_FragCoord.xy);

        if (ab_magnitude_squared > 0)
        {
            // t = point.z
            vec3 stroke_point = closest_point_in_segment_gl(VEC2(v_pointa.xy), VEC2(v_pointb.xy), ab, ab_magnitude_squared, fragment_point);
            float d = distance(u_stroke_point.xy, fragment_point);
            float t = u_stroke_point.z;
            float pressure = (1-t)*v_pointa.z + t*v_pointb.z;
            pressure /= float(PRESSURE_RESOLUTION_GL);
            float radius = pressure *  u_radius;
            bool inside = d < radius;;
            if (inside)
            {
                //gl_FragColor = blend(as_vec4(u_background_color), u_brush_color);
                //gl_FragColor = blend(as_vec4(u_background_color), u_brush_color);
                color = u_brush_color + VEC4(u_stroke.points[0].x);
                gl_FragColor = color;
            }
            else
            {

            }
        }
    }
#endif
    vec2 fragment_point = raster_to_canvas_gl(gl_FragCoord.xy);
    float min_dist = MAX_DIST;
    for (int point_i = 0; point_i < u_stroke.count-1; ++point_i)
    {
#if 1
        vec2 a = VEC2(u_stroke.points[point_i].xy);
        vec2 b = VEC2(u_stroke.points[point_i+1].xy);
#else
        vec2 a = VEC2(v_pointa.xy);
        vec2 b = VEC2(v_pointb.xy);
#endif
        vec2 ab = b - a;
        float ab_magnitude_squared = ab.x*ab.x + ab.y*ab.y;
        if (ab_magnitude_squared > 0)
        {
            vec3 stroke_point = closest_point_in_segment_gl(a, b, ab, ab_magnitude_squared, fragment_point);
            float d = distance(stroke_point.xy, fragment_point);
            float t = stroke_point.z;
            float pressure_a = u_stroke.points[point_i].z;
            float pressure_b = u_stroke.points[point_i+1].z;
            float pressure = (1-t)*pressure_a + t*pressure_b;
            pressure /= float(PRESSURE_RESOLUTION_GL);
            float radius = pressure *  u_radius;
            bool inside = d < radius;
            if (inside && d < min_dist)
            {
                min_dist = d;
                //gl_FragColor = blend(as_vec4(u_background_color), u_brush_color);
                //gl_FragColor = blend(as_vec4(u_background_color), u_brush_color);
                // TODO: break here? Faster or slower?
            }
        }
    }
    if (min_dist < MAX_DIST)
    {
        gl_FragColor = u_brush_color;
    }
}
//End
