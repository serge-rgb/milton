// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


in vec3 v_pointa;
in vec3 v_pointb;


#if HAS_TEXTURE_MULTISAMPLE
// TODO: Technically we can't specify this here since it is not the beginning of the shader.
#extension GL_ARB_sample_shading : enable
#extension GL_ARB_texture_multisample : enable
uniform sampler2DMS u_canvas;
#else
uniform sampler2D u_canvas;
#endif

#if __VERSION__ > 120
out vec4 out_color;
#endif

// x,y  - closest point
// z    - t in [0,1] interpolation value
vec3
closest_point_in_segment_gl(vec2 a, vec2 b,
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

    disc = clamp(disc, 0.0, mag_ab);
    result = vec3(a.x + disc * d_x, a.y + disc * d_y, disc / mag_ab);
    return result;
}

int
sample_stroke(vec2 point, vec3 a, vec3 b)
{
    int value = 0;
    // Check against a circle of pressure*brush_size at each point, which is cheap.
    float dist_a = distance(point, a.xy);
    float dist_b = distance(point, b.xy);
    float radius_a = float(a.z*u_radius);
    float radius_b = float(b.z*u_radius);
    if ( dist_a < radius_a || dist_b < radius_b ) {
        value = 1;
    }
    // If it's not inside the circle, it might be somewhere else in the stroke.
    else {
        vec2 ab = b.xy - a.xy;
        float ab_magnitude_squared = ab.x*ab.x + ab.y*ab.y;

        if ( ab_magnitude_squared > 0 ) {
            vec3 stroke_point = closest_point_in_segment_gl(a.xy, b.xy, ab, ab_magnitude_squared, point);
            // z coordinate of a and b has pressure values.
            // z coordinate of stroke_point has interpolation between them for closes point.
            float pressure = mix(a.z, b.z, stroke_point.z);
            if ( distance(stroke_point.xy, point) < u_radius*pressure ) {
                value = 1;
            }
        }
    }
    return value;
}

void
main()
{
    vec2 offset = vec2(0.0);

    #if HAS_TEXTURE_MULTISAMPLE
        #if defined(HAS_SAMPLE_SHADING)
            #if !defined(VENDOR_NVIDIA)
                offset = gl_SamplePosition - vec2(0.5);
            #endif
        #endif
    #endif

    vec2 screen_point = vec2(gl_FragCoord.x, u_screen_size.y - gl_FragCoord.y) + offset;

    int sample = sample_stroke(raster_to_canvas_gl(screen_point), v_pointa, v_pointb);

    if ( sample > 0 ) {
        // TODO: is there a way to do front-to-back rendering with a working eraser?
        if ( brush_is_eraser() ) {
            #if HAS_TEXTURE_MULTISAMPLE
                vec4 color = texelFetch(u_canvas, ivec2(gl_FragCoord.xy), gl_SampleID);
            #else
                vec2 coord = screen_point / u_screen_size;
                vec4 color = texture(u_canvas, coord);
            #endif
            out_color = color;
        }
        else {
            out_color = u_brush_color;
        }
    } else {
        discard;
    }
}//END