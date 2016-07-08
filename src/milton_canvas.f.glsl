// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


// MiltonState elements
uniform vec3 u_background_color;

// Per-stroke uniforms
uniform vec4 u_brush_color;

in float v_pressure;
flat in ivec3 v_pointa;


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
#define VEC4 vec4
#endif

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
    color.xyz = as_vec3(v_pointa);
    color.z /= float(PRESSURE_RESOLUTION_GL);
    /* color = VEC4(1,1,1,1) * (0.5f*float(float(color.z) / float(1 << 10))); */
    /* color.a = 1; */

    gl_FragColor = blend(as_vec4(u_background_color), color);
// TODO: check shader compiler warnings
}

