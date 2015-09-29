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


u32 color_v4f_to_u32(v4f c);

v4f color_u32_to_v4f(u32 color);

v4f color_rgb_to_rgba(v3f rgb, float a);

v4f blend_v4f(v4f dst, v4f src);

v3f hsv_to_rgb(v3f hsv);

v4f to_premultiplied(v3f rgb, f32 a);

v4f linear_to_sRGB_v4(v4f rgb);

v3f linear_to_sRGB(v3f rgb);

v3f sRGB_to_linear(v3f rgb);


