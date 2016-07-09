// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


in float v_pressure;
flat in ivec3 v_pointa;
flat in ivec3 v_pointb;


vec4 blend(vec4 dst, vec4 src)
{
    vec4 result = src + dst*(1.0f-src.a);
    //vec4 result = src + dst*(1.0-src.a);

    return result;
}

#define PRESSURE_RESOLUTION_GL (1<<20)


void main()
{
    vec4 test = VEC4(0.4,0,0.4,0.4);
    vec4 color = u_brush_color;
    color.xyz *= v_pointa.z;
    color.xyz /= float(PRESSURE_RESOLUTION_GL);

    vec2 ab = VEC2(v_pointb.xy - v_pointa.xy);
    float ab_magnitude_squared = ab.x*ab.x + ab.y*ab.y;

    vec2 fragment_point = canvas_to_raster_gl(gl_FragCoord.xy);
    if (ab_magnitude_squared > 0)
    {
        vec3 point = closest_point_in_segment_gl(VEC2(v_pointa.xy), VEC2(v_pointb.xy), ab, ab_magnitude_squared, fragment_point);
    }

    gl_FragColor = blend(as_vec4(u_background_color), color);
// TODO: check shader compiler warnings
}

