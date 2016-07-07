// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


// MiltonState elements
uniform vec3 u_background_color;

// Per-stroke uniforms
uniform vec4 u_brush_color;


#if GL_core_profile
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

void main()
{
    vec4 test = VEC4(0.4,0,0.4,0.4);
    gl_FragColor = blend(as_vec4(u_background_color), u_brush_color);
}

