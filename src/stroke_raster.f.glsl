// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


in vec3 v_pointa;
in vec3 v_pointb;

// #extension GL_ARB_sample_shading : enable
// #extension GL_ARB_texture_multisample : enable


#if HAS_MULTISAMPLE
#extension GL_ARB_sample_shading : enable
#extension GL_ARB_texture_multisample : enable
uniform sampler2DMS u_canvas;
#else
uniform sampler2D u_canvas;
#endif


out vec4 out_color;

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

int sample_stroke(vec2 point, vec3 a, vec3 b)
{
    int value = 0;

#if 1
    float dist_a = distance(point, a.xy);
    float dist_b = distance(point, b.xy);
    float radius_a = float(a.z*u_radius);
    float radius_b = float(b.z*u_radius);
    if (dist_a < radius_a || dist_b < radius_b)
    {
        value = 1;
    }
    else
#endif
    {
        vec2 ab = b.xy - a.xy;
        float ab_magnitude_squared = ab.x*ab.x + ab.y*ab.y;

        if (ab_magnitude_squared > 0)
        {
            vec3 stroke_point = closest_point_in_segment_gl(a.xy, b.xy, ab, ab_magnitude_squared, point);
            float d = distance(stroke_point.xy, point);
            float t = stroke_point.z;
            float pressure_a = a.z;
            float pressure_b = b.z;
            float pressure = (1-t)*pressure_a + t*pressure_b;
            float radius = pressure * u_radius;
            bool inside = d < radius;
            if (inside)
            {
                value = 1;
            }
        }
    }
    return value;
}

void main()
{
    vec2 offset = vec2(0.0);

    #if defined(HAS_SAMPLE_SHADING)
        #if !defined(VENDOR_NVIDIA)
            offset = gl_SamplePosition - vec2(0.5);
        #endif
    #endif

    vec2 screen_point = vec2(gl_FragCoord.x, u_screen_size.y - gl_FragCoord.y) + offset;
#if HAS_MULTISAMPLE
    vec4 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), gl_SampleID);
#else
    vec2 coord = gl_FragCoord.xy / u_screen_size;
    // vec4 color = texture(u_canvas, coord);
    vec4 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), 0);
#endif

    vec3 a = v_pointa;
    vec3 b = v_pointb;
    int sample = 0;
    // float subpixel_width = 0.25;
    // float subpixel_height = 0.25;

    sample = sample_stroke(raster_to_canvas_gl(screen_point), a, b);
    // sample = sample_stroke(raster_to_canvas_gl(screen_point + vec2(subpixel_width, subpixel_height)), a, b);
    // sample += sample_stroke(raster_to_canvas_gl(screen_point + vec2(subpixel_width, -subpixel_height)), a, b);
    // sample += sample_stroke(raster_to_canvas_gl(screen_point + vec2(-subpixel_width, -subpixel_height)), a, b);
    // sample += sample_stroke(raster_to_canvas_gl(screen_point + vec2(-subpixel_width, subpixel_height)), a, b);

    if (sample > 0)
    {
        // TODO: is there a way to do front-to-back rendering with a working eraser?
        out_color = brush_is_eraser() ? color : u_brush_color;
        // out_color.a = out_color.a * (sample/4.0);
    }
    else
    {
        discard;
    }
}
//End
