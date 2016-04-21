// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once


#include "utils.h"


u32 color_v4f_to_u32(v4f c);

v4f color_u32_to_v4f(u32 color);

v4f color_rgb_to_rgba(v3f rgb, float a);

v4f blend_v4f(v4f dst, v4f src);

v3f hsv_to_rgb(v3f hsv);

v3f rgb_to_hsv(v3f rgb);

v4f to_premultiplied(v3f rgb, f32 a);

v3f linear_to_sRGB(v3f rgb);

v3f sRGB_to_linear(v3f rgb);

v3f linear_to_square(v3f rgb);

v3f square_to_linear(v3f rgb);

v3f clamp_255(v3f color);

v3f clamp_01(v3f color);


