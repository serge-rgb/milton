// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "common.h"
#include "vector.h"

u32 color_v4f_to_u32(v4f c);

v4f color_u32_to_v4f(u32 color);

v4f color_rgb_to_rgba(v3f rgb, float a);

v4f blend_v4f(v4f dst, v4f src);

v3f hsv_to_rgb(v3f hsv);

v3f rgb_to_hsv(v3f rgb);

v4f to_premultiplied(v3f rgb, f32 a);

v3f clamp_255(v3f color);

v3f clamp_01(v3f color);

v4i color_u32_to_v4i(u32 color);

u32 color_v4i_to_u32(v4i color);

u32 un_premultiply(u32 in_color);


