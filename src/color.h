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


typedef enum
{
    ColorPickerFlags_NOTHING = 0,

    ColorPickerFlags_WHEEL_ACTIVE    = (1 << 1),
    ColorPickerFlags_TRIANGLE_ACTIVE = (1 << 2)
} ColorPickerFlags;

typedef struct ColorPicker_s
{
    v2f a;  // Corresponds to value = 0      (black)
    v2f b;  // Corresponds to saturation = 0 (white)
    v2f c;  // Points to chosen hue.         (full color)

    v2i     center;  // In screen pixel coordinates.
    i32   bound_radius_px;
    Rect    bounds;
    float   wheel_radius;
    float   wheel_half_width;

    u32* pixels;  // BLit this to render picker

    v3f     hsv;

    ColorPickerFlags flags;
} ColorPicker;

typedef enum
{
    ColorPickResult_NOTHING         = 0,
    ColorPickResult_CHANGE_COLOR    = (1 << 1),
} ColorPickResult;

typedef struct ColorManagement_s
{
    u32 mask_a;
    u32 mask_r;
    u32 mask_g;
    u32 mask_b;

    u32 shift_a;
    u32 shift_r;
    u32 shift_g;
    u32 shift_b;
} ColorManagement;

func void color_init(ColorManagement* cm)
{
    // TODO: This is platform dependant
    cm->mask_a = 0xff000000;
    cm->mask_r = 0x00ff0000;
    cm->mask_g = 0x0000ff00;
    cm->mask_b = 0x000000ff;
    cm->shift_a = find_least_significant_set_bit(cm->mask_a).index;
    cm->shift_r = find_least_significant_set_bit(cm->mask_r).index;
    cm->shift_g = find_least_significant_set_bit(cm->mask_g).index;
    cm->shift_b = find_least_significant_set_bit(cm->mask_b).index;
}

func u32 color_v4f_to_u32(ColorManagement cm, v4f c)
{
    u32 result =
        ((u8)(c.r * 255.0f) << cm.shift_r) |
        ((u8)(c.g * 255.0f) << cm.shift_g) |
        ((u8)(c.b * 255.0f) << cm.shift_b) |
        ((u8)(c.a * 255.0f) << cm.shift_a);
    return result;
}

func v4f color_u32_to_v4f(ColorManagement cm, u32 color)
{
    v4f result =
    {
        (float)(0xff & (color >> cm.shift_r)) / 255,
        (float)(0xff & (color >> cm.shift_g)) / 255,
        (float)(0xff & (color >> cm.shift_b)) / 255,
        (float)(0xff & (color >> cm.shift_a)) / 255,
    };

    return result;
}

func v4f color_rgb_to_rgba(v3f rgb, float a)
{
    return (v4f)
    {
        rgb.r,
        rgb.g,
        rgb.b,
        a
    };
}

func v3f hsv_to_rgb(v3f hsv)
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

    switch (hi)
    {
    case 0:
        {
            rgb.r = cr;
            rgb.g = x;
            rgb.b = 0;
            break;
        }
    case 1:
        {
            rgb.r = x;
            rgb.g = cr;
            rgb.b = 0;
            break;
        }
    case 2:
        {
            rgb.r = 0;
            rgb.g = cr;
            rgb.b = x;
            break;
        }
    case 3:
        {
            rgb.r = 0;
            rgb.g = x;
            rgb.b = cr;
            break;
        }
    case 4:
        {
            rgb.r = x;
            rgb.g = 0;
            rgb.b = cr;
            break;
        }
    case 5:
        {
            rgb.r = cr;
            rgb.g = 0;
            rgb.b = x;
            //  don't break;
        }
    default:
        {
            break;
        }
    }
    rgb.r += m;
    rgb.g += m;
    rgb.b += m;

    assert (rgb.r >= 0.0f && rgb.r <= 1.0f);
    assert (rgb.g >= 0.0f && rgb.g <= 1.0f);
    assert (rgb.b >= 0.0f && rgb.b <= 1.0f);
    return rgb;
}

func v4f to_premultiplied(v3f rgb, f32 a)
{
    v4f rgba =
    {
        .r = rgb.r * a,
        .g = rgb.g * a,
        .b = rgb.b * a,
        .a = a
    };
    return rgba;
}

#define FAST_GAMMA 1
func v4f linear_to_sRGB_v4(v4f rgb)
{
#if FAST_GAMMA
    v4f srgb =
    {
        sqrtf(rgb.r),
        sqrtf(rgb.g),
        sqrtf(rgb.b),
        rgb.a,
    };
#else
    v4f srgb =
    {
        powf(rgb.r, 1/2.22f),
        powf(rgb.g, 1/2.22f),
        powf(rgb.b, 1/2.22f),
        rgb.a,
    };
#endif
    return srgb;
}

func v3f linear_to_sRGB(v3f rgb)
{
#if FAST_GAMMA
    v3f srgb =
    {
        sqrtf(rgb.r),
        sqrtf(rgb.g),
        sqrtf(rgb.b),
    };
#else
    v3f srgb =
    {
        powf(rgb.r, 1/2.22f),
        powf(rgb.g, 1/2.22f),
        powf(rgb.b, 1/2.22f),
    };
#endif
    return srgb;
}

func v3f sRGB_to_linear(v3f rgb)
{
#if FAST_GAMMA
    v3f result =
    {
        rgb.r * rgb.r,
        rgb.g * rgb.g,
        rgb.b * rgb.b,
    };
#else
    v3f result = rgb;
    float* d = result.d;
    for (int i = 0; i < 3; ++i)
    {
        if (*d <= 0.04045f)
        {
            *d /= 12.92f;
        }
        else
        {
            *d = powf((*d + 0.055f) / 1.055f, 2.4f);
        }
        ++d;
    }
#endif
    return result;
}

func b32 picker_hits_wheel(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = sub_v2f (point, center);
    float dist = magnitude(arrow);
    if ((dist <= picker->wheel_radius + picker->wheel_half_width ) &&
        (dist >= picker->wheel_radius - picker->wheel_half_width ))
    {
        return true;
    }
    return false;
}

func float picker_wheel_get_angle(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = sub_v2f (point, center);
    v2f baseline = { 1, 0 };
    float dotp = (dot(arrow, baseline)) / (magnitude(arrow));
    float angle = acosf(dotp);
    if (point.y > center.y)
    {
        angle = (2 * kPi) - angle;
    }
    return angle;
}

func void picker_update_wheel(ColorPicker* picker, v2f point)
{
    float angle = picker_wheel_get_angle(picker, point);
    picker->hsv.h = radians_to_degrees(angle);
    // Update the triangle
    {
        float radius = 0.9f * (picker->wheel_radius - picker->wheel_half_width);
        v2f center = v2i_to_v2f(picker->center);
        {
            v2f point = polar_to_cartesian(-angle, radius);
            point = add_v2f(point, center);
            picker->c = point;
        }
        {
            v2f point = polar_to_cartesian(-angle + 2 * kPi / 3.0f, radius);
            point = add_v2f(point, center);
            picker->b = point;
        }
        {
            v2f point = polar_to_cartesian(-angle + 4 * kPi / 3.0f, radius);
            point = add_v2f(point, center);
            picker->a = point;
        }
    }
}


func b32 picker_hits_triangle(ColorPicker* picker, v2f fpoint)
{
    b32 result = is_inside_triangle(fpoint, picker->a, picker->b, picker->c);
    return result;
}

func void picker_deactivate(ColorPicker* picker)
{
    picker->flags = ColorPickerFlags_NOTHING;
}

func b32 is_inside_picker_rect(ColorPicker* picker, v2i point)
{
    return is_inside_rect(picker->bounds, point);
}

func b32 is_inside_picker_active_area(ColorPicker* picker, v2i point)
{
    v2f fpoint = v2i_to_v2f(point);
    b32 result = picker_hits_wheel(picker, fpoint) ||
            is_inside_triangle(fpoint, picker->a, picker->b, picker->c);
    return result;
}

func b32 is_picker_accepting_input(ColorPicker* picker, v2i point)
{
    // If wheel is active, yes! Gimme input.
    if (picker->flags & ColorPickerFlags_WHEEL_ACTIVE)
    {
        return true;
    }
    else
    {
        return is_inside_picker_active_area(picker, point);
    }
}

func Rect picker_get_bounds(ColorPicker* picker)
{
    Rect picker_rect;
    {
        picker_rect.left = picker->center.x - picker->bound_radius_px;
        picker_rect.right = picker->center.x + picker->bound_radius_px;
        picker_rect.bottom = picker->center.y + picker->bound_radius_px;
        picker_rect.top = picker->center.y - picker->bound_radius_px;
    }
    assert (picker_rect.left >= 0);
    assert (picker_rect.top >= 0);

    return picker_rect;
}

func v3f picker_hsv_from_point(ColorPicker* picker, v2f point)
{
    float area = orientation(picker->a, picker->b, picker->c);
    assert (area != 0);
    float inv_area = 1.0f / area;
    float s = orientation(picker->b, point, picker->a) * inv_area;
    if (s > 1) { s = 1; }
    if (s < 0) { s = 0; }
    float v = 1 - (orientation(point, picker->c, picker->a) * inv_area);
    if (v > 1) { v = 1; }
    if (v < 0) { v = 0; }

    v3f hsv =
    {
        picker->hsv.h,
        s,
        v,
    };
    return hsv;
}

func ColorPickResult picker_update(ColorPicker* picker, v2i point)
{
    ColorPickResult result = ColorPickResult_NOTHING;
    v2f fpoint = v2i_to_v2f(point);
    if (picker->flags == ColorPickResult_NOTHING)
    {
        if (picker_hits_wheel(picker, fpoint))
        {
            picker->flags |= ColorPickerFlags_WHEEL_ACTIVE;
        }
    }
    if (picker->flags & ColorPickerFlags_WHEEL_ACTIVE)
    {
        if (!(picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE))
        {
            picker_update_wheel(picker, fpoint);
            result |= ColorPickResult_CHANGE_COLOR;
        }
    }
    if (picker_hits_triangle(picker, fpoint))
    {
        if (!(picker->flags & ColorPickerFlags_WHEEL_ACTIVE))
        {
            picker->flags |= ColorPickerFlags_TRIANGLE_ACTIVE;
            picker->hsv = picker_hsv_from_point(picker, fpoint);
            result |= ColorPickResult_CHANGE_COLOR;
        }
    }

    return result;
}

func void picker_init(ColorPicker* picker)
{

    picker_update_wheel(picker, (v2f)
                        {
                        (f32)picker->center.x + (int)(picker->wheel_radius),
                        (f32)picker->center.y
                        });
}
