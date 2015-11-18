//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


u32 color_v4f_to_u32(v4f c)
{
    u32 result = (u32)
        ((int)(c.r * 255.0f) << 16) |
        ((int)(c.g * 255.0f) << 8) |
        ((int)(c.b * 255.0f) << 0) |
        ((int)(c.a * 255.0f) << 24);
    return result;
}

v4f color_u32_to_v4f(u32 color)
{
    v4f result = {
        (float)(0xff & (color >> 16)) / 255,
        (float)(0xff & (color >> 8)) / 255,
        (float)(0xff & (color >> 0)) / 255,
        (float)(0xff & (color >> 24)) / 255,
    };

    return result;
}

v4f color_rgb_to_rgba(v3f rgb, float a)
{
    v4f rgba = {
        rgb.r,
        rgb.g,
        rgb.b,
        a
    };
    return rgba;
}

v4f blend_v4f(v4f dst, v4f src)
{
    f32 alpha = 1 - ((1 - src.a) * (1 - dst.a));

    //f32 alpha = src.a + dst.a - (src.a * dst.a);
    v4f result = {
        src.r + dst.r * (1 - src.a),
        src.g + dst.g * (1 - src.a),
        src.b + dst.b * (1 - src.a),
        alpha
    };

    return result;
}

v3f hsv_to_rgb(v3f hsv)
{
    v3f rgb = { 0 };

    float h = hsv.x;
    float s = hsv.y;
    float v = hsv.z;
    float hh = h / 60.0f;
    int hi = (int)(floor(hh));
    float cr = v * s;
    float rem = fmodf(hh, 2.0f);
    float x = cr * (1.0f - fabsf(rem - 1.0f));
    float m = v - cr;

    switch (hi) {
    case 0:
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

    assert (rgb.r >= 0.0f && rgb.r <= 1.0f);
    assert (rgb.g >= 0.0f && rgb.g <= 1.0f);
    assert (rgb.b >= 0.0f && rgb.b <= 1.0f);
    return rgb;
}

v4f to_premultiplied(v3f rgb, f32 a)
{
    v4f rgba = {
        rgb.r * a,
        rgb.g * a,
        rgb.b * a,
        a
    };
    return rgba;
}

#define FAST_GAMMA 1
v4f linear_to_sRGB_v4(v4f rgb)
{
#if FAST_GAMMA
    v4f srgb = {
        sqrtf(rgb.r),
        sqrtf(rgb.g),
        sqrtf(rgb.b),
        rgb.a,
    };
#else
    v4f srgb = {
        powf(rgb.r, 1/2.22f),
        powf(rgb.g, 1/2.22f),
        powf(rgb.b, 1/2.22f),
        rgb.a,
    };
#endif
    return srgb;
}

v3f linear_to_sRGB(v3f rgb)
{
#if FAST_GAMMA
    v3f srgb = {
        sqrtf(rgb.r),
        sqrtf(rgb.g),
        sqrtf(rgb.b),
    };
#else
    v3f srgb = {
        powf(rgb.r, 1/2.22f),
        powf(rgb.g, 1/2.22f),
        powf(rgb.b, 1/2.22f),
    };
#endif
    return srgb;
}

v3f sRGB_to_linear(v3f rgb)
{
#if FAST_GAMMA
    v3f result = {
        rgb.r * rgb.r,
        rgb.g * rgb.g,
        rgb.b * rgb.b,
    };
#else
    v3f result = rgb;
    float* d = result.d;
    for (int i = 0; i < 3; ++i) {
        if (*d <= 0.04045f) {
            *d /= 12.92f;
        } else {
            *d = powf((*d + 0.055f) / 1.055f, 2.4f);
        }
        ++d;
    }
#endif
    return result;
}

