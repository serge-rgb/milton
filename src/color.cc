// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "color.h"

#include "common.h"
#include "utils.h"


u32
color_v4f_to_u32(v4f c)
{
    u32 result = (u32)
        ((int)(c.r * 255.0f) << 0) |
        ((int)(c.g * 255.0f) << 8) |
        ((int)(c.b * 255.0f) << 16) |
        ((int)(c.a * 255.0f) << 24);
    return result;
}

v4f
color_u32_to_v4f(u32 color)
{
    v4f result = {
        (float)(0xff & (color >> 0)) / 255,
        (float)(0xff & (color >> 8)) / 255,
        (float)(0xff & (color >> 16)) / 255,
        (float)(0xff & (color >> 24)) / 255,
    };

    return result;
}

v4f
color_rgb_to_rgba(v3f rgb, float a)
{
    v4f rgba = {
        rgb.r,
        rgb.g,
        rgb.b,
        a
    };
    return rgba;
}

// src over dst
v4f
blend_v4f(v4f dst, v4f src)
{
    //f32 alpha = 1 - ((1 - src.a) * (1 - dst.a));

    //f32 alpha = src.a + dst.a - (src.a * dst.a);
    v4f result = {
        src.r + dst.r * (1 - src.a),
        src.g + dst.g * (1 - src.a),
        src.b + dst.b * (1 - src.a),
        src.a + dst.a * (1 - src.a),
    };

    return result;
}

v3f
clamp_01(v3f color)
{
    v3f result = color;
    for ( int i = 0; i < 3; ++i ) {
        if ( result.d[i] > 1.0f ) {
            result.d[i] = 1.0f;
        }
        if ( result.d[i] < 0.0f ) {
            result.d[i] = 0.0f;
        }
    }
    return result;
}

v3f
clamp_255(v3f color)
{
    v3f result = color;
    for ( int i = 0; i < 3; ++i ) {
        if ( result.d[i] > 255.0f ) {
            result.d[i] = 255.0f;
        }
        if ( result.d[i] < 0.0f ) {
            result.d[i] = 0.0f;
        }
    }
    return result;
}

v3f
rgb_to_hsv(v3f rgb)
{
    v3f hsv = {};
    float fmin = min( rgb.r, min(rgb.g, rgb.b) );
    float fmax = max( rgb.r, max(rgb.g, rgb.b) );
    hsv.v = fmax;
    float chroma = fmax - fmin; // in [0,1]

    // Dividing every sector by 2 so that the hexagon length is 1.0
    if ( chroma != 0 ) {
        if ( rgb.r == fmax ) {
            // R sector
            hsv.h = (rgb.g - rgb.b) / chroma;
            if ( hsv.h < 0.0f ) {
                hsv.h += 6.0f;
            }
        }
        else if ( rgb.g == fmax ) {
            // G sector
            hsv.h = ((rgb.b - rgb.r) / chroma) + 2.0f;
        }
        else {
            // B sector
            hsv.h = ((rgb.r - rgb.g) / chroma) + 4.0f;
        }
        hsv.h /= 6.0f;

        hsv.s = chroma / fmax;
    }
    hsv.v = fmax;

    for ( int i=0;i<3;++i ) {
        if (hsv.d[i] < 0) hsv.d[i]=0;
        if (hsv.d[i] > 1) hsv.d[i]=1;
    }
    return hsv;
}

v3f
hsv_to_rgb(v3f hsv)
{
    v3f rgb = {};

    float h = hsv.x;
    float s = hsv.y;
    float v = hsv.z;
    float hh = h / 60.0f;
    int hi = (int)(floor(hh));
    float cr = v * s;
    float rem = fmodf(hh, 2.0f);
    float x = cr * (1.0f - fabsf(rem - 1.0f));
    float m = v - cr;

    switch ( hi ) {
    case 0:
    case 6:
        rgb.r = cr;
        rgb.g = x;
        rgb.b = 0;
        break;
    case 1:
        rgb.r = x;
        rgb.g = cr;
        rgb.b = 0;
        break;
    case 2:
        rgb.r = 0;
        rgb.g = cr;
        rgb.b = x;
        break;
    case 3:
        rgb.r = 0;
        rgb.g = x;
        rgb.b = cr;
        break;
    case 4:
        rgb.r = x;
        rgb.g = 0;
        rgb.b = cr;
        break;
    case 5:
        rgb.r = cr;
        rgb.g = 0;
        rgb.b = x;
        break;
    default:
        break;
    }
    rgb.r += m;
    rgb.g += m;
    rgb.b += m;

    mlt_assert (rgb.r >= 0.0f && rgb.r <= 1.0f);
    mlt_assert (rgb.g >= 0.0f && rgb.g <= 1.0f);
    mlt_assert (rgb.b >= 0.0f && rgb.b <= 1.0f);
    return rgb;
}

v4f
to_premultiplied(v3f rgb, f32 a)
{
    v4f rgba = {
        rgb.r * a,
        rgb.g * a,
        rgb.b * a,
        a
    };
    return rgba;
}

v4i color_u32_to_v4i(u32 color)
{
    v4i c = {};

    c.r = (i32)(color >> 24);
    c.g = (i32)(color >> 16) & 255;
    c.b = (i32)(color >> 8) & 255;
    c.a = (i32)(color) & 255;

    return c;
}

u32 color_v4i_to_u32(v4i color)
{
    u32 c = 0;

    // mlt_assert(color.r <= 255);
    // mlt_assert(color.g <= 255);
    // mlt_assert(color.b <= 255);
    // mlt_assert(color.a <= 255);

    c = ((u32)color.r<<24) | ((u32)color.g<<16) | ((u32)color.b<<8) | ((u32)color.a);

    return c;
}

u32
un_premultiply(u32 in_color)
{
    u32 out_color = 0;
    v4i v = color_u32_to_v4i(in_color);
    if ( v.a != 0 ) {
        v.r = (int)(v.r / (v.a/255.0));
        v.g = (int)(v.g / (v.a/255.0));
        v.b = (int)(v.b / (v.a/255.0));
    }
    out_color = color_v4i_to_u32(v);
    return out_color;
}
