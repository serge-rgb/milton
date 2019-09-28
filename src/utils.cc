// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "system_includes.h"

#include "common.h"
#include "DArray.h"
#include "memory.h"
#include "utils.h"


// total RAM in bytes
size_t
get_system_RAM()
{
    return (size_t)SDL_GetSystemRAM() * 1024 * 1024;
}

v2l
v2f_to_v2l(v2f p)
{
    return v2l{(i64)p.x, (i64)p.y};
}

v2i
v2l_to_v2i(v2l p)
{
    return v2i{ (i32)p.x, (i32)p.y };
}


v2f
v2l_to_v2f(v2l p)
{
    #ifdef _WIN32
    #pragma warning(push)
    #pragma warning(disable: 4307)
    #endif
    mlt_assert(MLT_ABS(p.x) < (1L<<31) - 1L);
    mlt_assert(MLT_ABS(p.y) < (1L<<31) - 1L);
    #ifdef _WIN32
    #pragma warning(pop)
    #endif
    return v2f{(f32)p.x, (f32)p.y};
}

v2l
v2i_to_v2l(v2i p)
{
    return v2l{p.x, p.y};
}

v2f
v2i_to_v2f(v2i p)
{
    return v2f{(f32)p.x, (f32)p.y};
}


// ---------------
// Math functions.
// ---------------

f32
magnitude(v2f a)
{
    return sqrtf(DOT(a, a));
}

i64
magnitude(v2l a)
{
    return sqrtf((float)DOT(a, a));
}

f32
distance(v2f a, v2f b)
{
    v2f diff = a - b;

    f32 dist = sqrtf(DOT(diff, diff));

    return dist;
}

i32
manhattan_distance(v2i a, v2i b)
{
    i32 dist = MLT_ABS(a.x - b.x) + MLT_ABS(a.y - b.y);
    return dist;
}

f32
deegrees_to_radians(int d)
{
    mlt_assert (0 <= d && d < 360);
    return kPi * ((f32)(d) / 180.0f);
}

f32
radians_to_degrees(f32 r)
{
    return (180 * r) / kPi;
}

f32
norm(v2f v)
{
    f32 result = sqrtf(v.x * v.x + v.y * v.y);
    return result;
}

v2f
normalized (v2f v)
{
    v2f result = v / norm(v);
    return result;
}

f32
clamp(f32 value, f32 min, f32 max)
{
    return min(max(value, min), max);
}



// Could be called a signed area. `orientation(a, b, c) / 2` is the area of the
// triangle.
// If positive, c is to the left of ab. Negative: right of ab. 0 if
// colinear.
f32
orientation(v2f a, v2f b, v2f c)
{
    return (b.x - a.x)*(c.y - a.y) - (c.x - a.x)*(b.y - a.y);
}

b32
is_inside_triangle(v2f point, v2f a, v2f b, v2f c)
{
    b32 is_inside =
            (orientation(a, b, point) <= 0) &&
            (orientation(b, c, point) <= 0) &&
            (orientation(c, a, point) <= 0);
    return is_inside;
}

v2f
polar_to_cartesian(f32 angle, f32 radius)
{
    v2f result = {
        radius * cosf(angle),
        radius * sinf(angle)
    };
    return result;
}

v2i
rotate_v2i(v2i p, f32 angle)
{
    v2i r = {
        (i32)((p.x * cosf(angle)) - (p.y * sinf(angle))),
        (i32)((p.x * sinf(angle)) + (p.y * cosf(angle))),
    };
    return r;
}

v2f
closest_point_in_segment_f(i32 ax, i32 ay,
                           i32 bx, i32 by,
                           v2f ab, f32 ab_magnitude_squared,
                           v2i point, f32* out_t)
{
    v2f result;
    f32 mag_ab = sqrtf(ab_magnitude_squared);
    f32 d_x = ab.x / mag_ab;
    f32 d_y = ab.y / mag_ab;
    f32 ax_x = (f32)(point.x - ax);
    f32 ax_y = (f32)(point.y - ay);
    f32 disc = d_x * ax_x + d_y * ax_y;
    if ( disc < 0 ) disc = 0;
    if ( disc > mag_ab ) disc = mag_ab;
    if ( out_t ) {
        *out_t = disc / mag_ab;
    }
    result = v2f{(ax + disc * d_x), (ay + disc * d_y)};
    return result;
}

v2i
closest_point_in_segment(v2i a, v2i b,
                             v2f ab, f32 ab_magnitude_squared,
                             v2i point, f32* out_t)
{
    v2i result;
    f32 mag_ab = sqrtf(ab_magnitude_squared);
    f32 d_x = ab.x / mag_ab;
    f32 d_y = ab.y / mag_ab;
    f32 ax_x = (f32)(point.x - a.x);
    f32 ax_y = (f32)(point.y - a.y);
    f32 disc = d_x * ax_x + d_y * ax_y;
    if ( disc < 0 ) disc = 0;
    if ( disc > mag_ab ) disc = mag_ab;
    if ( out_t ) {
        *out_t = disc / mag_ab;
    }
    result = v2i{(i32)(a.x + disc * d_x), (i32)(a.y + disc * d_y)};
    return result;
}

b32
intersect_line_segments(v2i a, v2i b,
                            v2i u, v2i v,
                            v2f* out_intersection)
{
    b32 hit = false;
    v2i perp = perpendicular(v - u);
    i32 det = DOT(b - a, perp);
    if ( det != 0 ) {
        f32 t = (f32)DOT(u - a, perp) / (f32)det;
        if (t > 1 && t < 1.001) t = 1;
        if (t < 0 && t > -0.001) t = 1;

        if ( t >= 0 && t <= 1 ) {
            hit = true;
            out_intersection->x = a.x + t * (b.x - a.x);
            out_intersection->y = a.y + t * (b.y - a.y);
        }
    }
    return hit;
}

i32
rect_split(Rect** out_rects, Rect src_rect, i32 width, i32 height)
{
    DArray<Rect> rects = {};
    reserve(&rects, 32);

    int n_width = (src_rect.right - src_rect.left) / width;
    int n_height = (src_rect.bottom - src_rect.top) / height;

    if ( !n_width || !n_height ) {
        return 0;
    }

    i32 max_num_rects = (n_width + 1) * (n_height + 1);

    for ( int h = src_rect.top; h < src_rect.bottom; h += height ) {
        for ( int w = src_rect.left; w < src_rect.right; w += width ) {
            Rect rect;
            {
                rect.left = w;
                rect.right = min(src_rect.right, w + width);
                rect.top = h;
                rect.bottom = min(src_rect.bottom, h + height);
            }
            push(&rects, rect);
        }
    }

    mlt_assert((i32)rects.count <= max_num_rects);
    *out_rects = rects.data;
    i32 num_rects = (i32)rects.count;
    return num_rects;
}

Rect
rect_union(Rect a, Rect b)
{
    Rect result;
    result.left = min(a.left, b.left);
    result.right = max(a.right, b.right);

    if ( result.left > result.right ) {
        result.left = result.right;
    }
    result.top = min(a.top, b.top);
    result.bottom = max(a.bottom, b.bottom);
    if ( result.bottom < result.top ) {
        result.bottom = result.top;
    }
    return result;
}

b32
rect_intersects_rect(Rect a, Rect b)
{
    b32 intersects = true;
    if ( a.left > b.right || b.left > a.right ||
         a.top > b.bottom || b.top > a.bottom ) {
        intersects = false;
    }
    return intersects;
}

Rect
rect_intersect(Rect a, Rect b)
{
    Rect result;
    result.left = max(a.left, b.left);
    result.right = min(a.right, b.right);

    if ( result.left >= result.right ) {
        result.left = result.right;
    }
    result.top = max(a.top, b.top);
    result.bottom = min(a.bottom, b.bottom);
    if ( result.bottom <= result.top ) {
        result.bottom = result.top;
    }
    return result;
}
Rect
rect_stretch(Rect rect, i32 width)
{
    Rect stretched = rect;
    // Make the raster limits at least as wide as a block
    if ( stretched.bottom - stretched.top < width ) {
        stretched.top -= width / 2;
        stretched.bottom += width / 2;
    }
    if ( stretched.right - stretched.left < width ) {
        stretched.left -= width / 2;
        stretched.right += width / 2;
    }
    return stretched;
}

Rect
rect_clip_to_screen(Rect limits, v2i screen_size)
{
    if (limits.left < 0) limits.left = 0;
    if (limits.right > screen_size.w) limits.right = screen_size.w;
    if (limits.top < 0) limits.top = 0;
    if (limits.bottom > screen_size.h) limits.bottom = screen_size.h;
    return limits;
}

const Rect
rect_enlarge(Rect src, i32 offset)
{
    Rect result;
    result.left = src.left - offset;
    result.top = src.top - offset;
    result.right = src.right + offset;
    result.bottom = src.bottom + offset;
    return result;
}

Rect
bounding_rect_for_points(v2l points[], i32 num_points)
{
    mlt_assert (num_points > 0);

    v2l top_left =  points[0];
    v2l bot_right = points[0];

    for ( i32 i = 1; i < num_points; ++i ) {
        v2l point = points[i];
        if (point.x < top_left.x)   top_left.x = point.x;
        if (point.x > bot_right.x)  bot_right.x = point.x;

        if (point.y < top_left.y)   top_left.y = point.y;
        if (point.y > bot_right.y)  bot_right.y = point.y;
    }
    Rect rect = { top_left, bot_right };
    return rect;
}

b32
is_inside_rect(Rect bounds, v2i point)
{
    return
        point.x >= bounds.left &&
        point.x <  bounds.right &&
        point.y >= bounds.top &&
        point.y <  bounds.bottom;
}

b32
rect_is_valid(Rect rect)
{
    b32 valid = rect.left <= rect.right && rect.top <= rect.bottom;
    return valid;
}

Rect
bounding_rect_for_points_scalar(i32 points_x[], i32 points_y[], i32 num_points)
{
    mlt_assert (num_points > 0);

    i32 top_left_x =  points_x[0];
    i32 bot_right_x = points_x[0];

    i32 top_left_y =  points_y[0];
    i32 bot_right_y = points_y[0];

    for ( i32 i = 1; i < num_points; ++i ) {
        if (points_x[i] < top_left_x)   top_left_x = points_x[i];
        if (points_x[i] > bot_right_x)  bot_right_x = points_x[i];

        if (points_y[i] < top_left_y)   top_left_y = points_y[i];
        if (points_y[i] > bot_right_y)  bot_right_y = points_y[i];
    }
    Rect rect;
        rect.left = top_left_x;
        rect.right = bot_right_x;
        rect.top = top_left_y;
        rect.bottom = bot_right_y;
    return rect;
}

Rect
rect_without_size()
{
    Rect rect;
    rect.left = I64_MAX;
    rect.right = I64_MIN;
    rect.top = I64_MAX;
    rect.bottom = I64_MIN;
    return rect;
}

i32
rect_area(Rect rect)
{
    return (rect.right - rect.left) * (rect.bottom - rect.top);
}

b32
is_inside_rect_scalar(Rect bounds, i32 point_x, i32 point_y)
{
    return
        point_x >= bounds.left &&
        point_x <  bounds.right &&
        point_y >= bounds.top &&
        point_y <  bounds.bottom;
}

b32
is_rect_within_rect(Rect a, Rect b)
{
    if (    (a.left   < b.left)
         || (a.right  > b.right)
         || (a.top    < b.top)
         || (a.bottom > b.bottom) ) {
        return false;
    }
    return true;
}

Rect
rect_from_xywh(i32 x, i32 y, i32 w, i32 h)
{
    Rect rect;
    rect.left = x;
    rect.right = x + w;
    rect.top = y;
    rect.bottom = y + h;

    return rect;
}

void
utf16_to_utf8_simple(char* , char* )
{
    // Nothing needs to be done
}

void
utf16_to_utf8_simple(wchar_t* utf16_name, char* utf8_name)
{
    for ( wchar_t* iter = utf16_name;
          *iter!='\0';
          ++iter ) {
        if ( *iter <= 128 ) {
            *utf8_name++ = (char)*iter;
        }
    }
    *utf8_name='\0';
}

wchar_t*
str_trim_to_last_slash(wchar_t* str)
{
    wchar_t* cool_char = str;
    for ( wchar_t* iter = str;
          *iter != '\0';
          iter++ ) {
        if ( (*iter == '/' || *iter == '\\') ) {
            cool_char = (iter+1);
        }
    }
    return cool_char;
}

char*
str_trim_to_last_slash(char* str)
{
    char* cool_char = str;
    for ( char* iter = str;
          *iter != '\0';
          iter++ ) {
        if ( (*iter == '/' || *iter == '\\') ) {
            cool_char = (iter+1);
        }
    }
    return cool_char;
}

u64
hash(char* string, size_t len)
{
    u64 h = 0;
    for ( size_t i = 0; i < len; ++i ) {
        h += string[i];
        h ^= h<<10;
        h += h>>5;
    }
    return h;
}

u64
difference_in_ms(WallTime start, WallTime end)
{
    // Note: There is some risk of overflow here, but at the time of my
    // writing this we are only using this for animations, where things won't
    // explode.

    u64 diff = end.ms - start.ms;
    if (end.s > start.s) {
        diff += 1000 * (end.s - start.s);
    }
    if (end.m > start.m) {
        diff += 1000 * 60 * (end.m - start.s);
    }
    if (end.h > start.h) {
        diff += 1000 * 60 * 60 * (end.h - start.h);
    }
    return diff;
}