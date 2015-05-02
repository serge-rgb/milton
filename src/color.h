// color.h
// (c) Copyright 2015 by Sergio Gonzalez.

typedef struct ColorPicker_s
{
    v2f a;  // Corresponds to value = 0      (black)
    v2f b;  // Corresponds to saturation = 0 (white)
    v2f c;  // Points to chosen hue.         (full color)

    v2i center;  // In screen pixel coordinates.
    int32 bound_radius_px;
    Rect bound;
    float wheel_radius;
    float wheel_half_width;
} ColorPicker;

typedef enum
{
    ColorPickResult_nothing         = 0,
    ColorPickResult_change_color    = (1 << 1),
    ColorPickResult_redraw_picker   = (1 << 2),
} ColorPickResult;


v3f hsv_to_rgb(v3f hsv)
{
    v3f rgb = { 0 };

    float h = hsv.x;
    float s = hsv.y;
    float v = hsv.z;
    float hh = h / 60.0f;
    int hi = (int)(hh);
    float cr = v * s;
    float x = cr * (1.0f - absf((fmodf(hh, 2.0f)) - 1.0f));
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
    return rgb;
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

static bool32 is_inside_picker(ColorPicker* picker, v2i point)
{
    return false;
}

static ColorPickResult picker_update(ColorPicker* picker, v2i point)
{
    return ColorPickResult_nothing;
}
