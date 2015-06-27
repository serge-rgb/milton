// color.h
// (c) Copyright 2015 by Sergio Gonzalez.

typedef enum
{
    ColorPickerFlags_none,

    ColorPickerFlags_wheel_active   = (1 << 0),
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
    ColorPickResult_nothing         = 0,
    ColorPickResult_change_color    = (1 << 1),
    ColorPickResult_redraw_picker   = (1 << 2),
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

static void color_init(ColorManagement* cm)
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

inline u32 color_v4f_to_u32(ColorManagement cm, v4f c)
{
    u32 result =
        ((u8)(c.r * 255.0f) << cm.shift_r) |
        ((u8)(c.g * 255.0f) << cm.shift_g) |
        ((u8)(c.b * 255.0f) << cm.shift_b) |
        ((u8)(c.a * 255.0f) << cm.shift_a);
    return result;
}

inline v4f color_u32_to_v4f(ColorManagement cm, u32 color)
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

inline v4f color_rgb_to_rgba(v3f rgb, float a)
{
    return (v4f)
    {
        rgb.r,
        rgb.g,
        rgb.b,
        a
    };
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
    float x = cr * (1.0f - absf(rem - 1.0f));
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

inline v4f linear_to_sRGB_v4(v4f rgb)
{
    v4f srgb =
    {
        powf(rgb.r, 1/2.22f),
        powf(rgb.g, 1/2.22f),
        powf(rgb.b, 1/2.22f),
        rgb.a,
    };
    return srgb;
}

inline v3f linear_to_sRGB(v3f rgb)
{
    v3f srgb =
    {
        powf(rgb.r, 1/2.22f),
        powf(rgb.g, 1/2.22f),
        powf(rgb.b, 1/2.22f),
    };
    return srgb;
}

inline v3f sRGB_to_linear(v3f rgb)
{
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
    return result;
}

static b32 picker_wheel_active(ColorPicker* picker)
{
    return (picker->flags & ColorPickerFlags_wheel_active);
}

static void picker_wheel_deactivate(ColorPicker* picker)
{
    picker->flags &= ~ColorPickerFlags_wheel_active;
}

static float picker_wheel_get_angle(ColorPicker* picker, v2f point)
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
static b32 picker_is_within_wheel(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = sub_v2f (point, center);
    float dist = magnitude(arrow);
    if ((dist <= picker->wheel_radius - picker->wheel_half_width ))
    {
        return true;
    }
    return false;
}

static b32 picker_hits_wheel(ColorPicker* picker, v2f point)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = sub_v2f (point, center);
    float dist = magnitude(arrow);
    if (
            (dist <= picker->wheel_radius + picker->wheel_half_width ) &&
            (dist >= picker->wheel_radius - picker->wheel_half_width )
       )
    {
        picker->flags |= ColorPickerFlags_wheel_active;
        return true;
    }
    return false;
}

static b32 is_inside_picker(ColorPicker* picker, v2i point)
{
    return is_inside_rect(picker->bounds, point);
}

static Rect picker_get_bounds(ColorPicker* picker)
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

static void picker_update_wheel(ColorPicker* picker, v2f point)
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

inline v3f picker_hsv_from_point(ColorPicker* picker, v2f point)
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

static ColorPickResult picker_update(ColorPicker* picker, v2i point)
{
    ColorPickResult result = ColorPickResult_nothing;
    v2f fpoint = v2i_to_v2f(point);
    float angle = 0;
    if (picker_hits_wheel(picker, fpoint))
    {
        picker_update_wheel(picker, fpoint);
        result |= ColorPickResult_change_color;
    }
    if (is_inside_triangle(fpoint, picker->a, picker->b, picker->c))
    {
        picker->hsv = picker_hsv_from_point(picker, fpoint);
        result |= ColorPickResult_change_color;
    }

    return result;
}
