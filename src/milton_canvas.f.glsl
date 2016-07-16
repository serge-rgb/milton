// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// TODO: this layout qualifier introduces GLSL 150 dependency.
layout(origin_upper_left) in vec4 gl_FragCoord;

flat in vec3 v_pointa;
flat in vec3 v_pointb;
uniform sampler2D u_canvas;


#define PRESSURE_RESOLUTION_GL (1<<20)

//  TODO: Uniform buffer objets are our only relatively non-hairy choice.  Any
//  GPU will run out of memory with a big enough drawing when allocating a
//  fixed-size UBO per-stroke, so there is going to be a change. One
//  possibility is to render drawings in multiple passes, storing a buffer for
//  the painting for a certain number of strokes.  This makes sense since we
//  avoid duplicating rendering work for the most common case which is not
//  panning and not zooming, just drawing over the existing painting.
#define STROKE_MAX_POINTS_GL 256
#if GL_core_profile
// TODO: instead of std140, get offsets with glGetUniformIndices and glGetActiveUNiformsiv GL_UNIFORM_OFFSET
layout(std140) uniform StrokeUniformBlock
{
    int count;
    ivec3 points[STROKE_MAX_POINTS_GL];  // 8KB
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

#if 0
    if (disc < 0.0)
    {
        disc = 0.0;
    }
    else if (disc > mag_ab)
    {
        disc = mag_ab;
    }
#else
    float ltz = float(disc < 0.0);
    disc = ltz*0.0 + (1-ltz)*disc;
    float gt = float(disc > mag_ab);
    disc = gt*mag_ab + (1-gt)*disc;
#endif
    result = VEC3(a.x + disc * d_x, a.y + disc * d_y, disc / mag_ab);
    return result;
}

#define MAX_DIST (1<<24)

void main()
{
    vec2 coord = gl_FragCoord.xy / u_screen_size;
    coord.y = 1-coord.y;
    vec4 color = texture2D(u_canvas, coord);

    //if (color.a == 0) { discard; }

    vec2 fragment_point = raster_to_canvas_gl(gl_FragCoord.xy);
    bool found = false;
    vec3 a = v_pointa;
    vec3 b = v_pointb;

#if 1
    // Most points are going to be within one of the two circles.
    if (distance(a.xy, fragment_point) < u_radius*a.z/float(PRESSURE_RESOLUTION_GL) ||
        distance(b.xy, fragment_point) < u_radius*b.z/float(PRESSURE_RESOLUTION_GL))
    {
        // BLEND
        color = blend(color, u_brush_color);
        found = true;
    }
    else
    {
        vec2 ab = b.xy - a.xy;
        float ab_magnitude_squared = ab.x*ab.x + ab.y*ab.y;
        if (ab_magnitude_squared > 0)
        {
            vec3 stroke_point = closest_point_in_segment_gl(a.xy, b.xy, ab, ab_magnitude_squared, fragment_point);
            float d = distance(stroke_point.xy, fragment_point);
            float t = stroke_point.z;
            float pressure_a = a.z;
            float pressure_b = b.z;
            float pressure = (1-t)*pressure_a + t*pressure_b;
            pressure /= float(PRESSURE_RESOLUTION_GL);
            float radius = pressure * u_radius;
            bool inside = d < radius;
            if (inside)
            {
                // BLEND
                color = blend(color, u_brush_color);
                found = true;
            }
        }
    }
#endif
#if 0
    if (!found) for (int point_i = 0; point_i < u_stroke.count-1; ++point_i)
    {
        vec3 a = u_stroke.points[point_i];
        vec3 b = u_stroke.points[point_i+1];

        // Most points are going to be within one of the two circles.
        if (distance(a.xy, fragment_point) < u_radius*a.z/float(PRESSURE_RESOLUTION_GL) ||
            distance(b.xy, fragment_point) < u_radius*b.z/float(PRESSURE_RESOLUTION_GL))
        {
            // BLEND
            color = blend(color, u_brush_color);
            found = true;
            break;
        }

        vec2 ab = b.xy - a.xy;
        float ab_magnitude_squared = ab.x*ab.x + ab.y*ab.y;
        if (ab_magnitude_squared > 0)
        {
            vec3 stroke_point = closest_point_in_segment_gl(a.xy, b.xy, ab, ab_magnitude_squared, fragment_point);
            float d = distance(stroke_point.xy, fragment_point);
            float t = stroke_point.z;
            float pressure_a = a.z;
            float pressure_b = b.z;
            float pressure = (1-t)*pressure_a + t*pressure_b;
            pressure /= float(PRESSURE_RESOLUTION_GL);
            float radius = pressure * u_radius;
            bool inside = d < radius;
            if (inside)
            {
                // BLEND
                color = blend(color, u_brush_color);
                found = true;
                break;
            }
        }
    }
#endif
    if (found)
    {
        gl_FragColor = color;
    } else { discard; }
}
//End
