// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "vector.h"

#include "system_includes.h"  // Including this because some system headers will redefine macros. (offsetof in stddef.h)

#ifdef array_count
#error "array_count is already defined"
#else
#define array_count(arr) (sizeof((arr)) / sizeof((arr)[0]))
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? a : b)
#endif

#ifndef max
#define max(a, b) (((a) < (b)) ? b : a)
#endif

#ifndef offsetof
#define offsetof(object, member) ((size_t)&(((object *)0)->member))
#endif

// -----------
// System stuf
// -----------

// total RAM in bytes
size_t get_system_RAM();

// ---------------
// Math functions.
// ---------------

v2l v2f_to_v2l(v2f p);

v2f v2l_to_v2f(v2l p);

v2f v2i_to_v2f(v2i p);

v2i v2l_to_v2i(v2l p);

v2l v2i_to_v2l(v2i p);

#define kPi 3.14152654f

#define DOT(a, b)  ((a).x * (b).x + (a).y * (b).y)

#define I64_MAX 9223372036854775807L
#define I64_MIN -9223372036854775807L

f32 magnitude(v2f a);

f32 distance(v2f a, v2f b);

i32 manhattan_distance(v2i a, v2i b);

f32 deegrees_to_radians(int d);

f32 radians_to_degrees(f32 r);

f32 norm(v2f v);

v2f normalized (v2f v);

f32 clamp(f32 value, f32 min, f32 max);

#define SQUARE(x) ((x) * (x))


// Could be called a signed area. `orientation(a, b, c) / 2` is the area of the
// triangle.
// If positive, c is to the left of ab. Negative: right of ab. 0 if
// colinear.
f32 orientation(v2f a, v2f b, v2f c);

b32 is_inside_triangle(v2f point, v2f a, v2f b, v2f c);

v2f polar_to_cartesian(f32 angle, f32 radius);

v2i rotate_v2i(v2i p, f32 angle);

v2f closest_point_in_segment_f(i32 ax, i32 ay,
                               i32 bx, i32 by,
                               v2f ab, f32 ab_magnitude_squared,
                               v2i point, f32* out_t);
v2i closest_point_in_segment(v2i a, v2i b,
                             v2f ab, f32 ab_magnitude_squared,
                             v2i point, f32* out_t);

// Returns false if there is no intersection.
// Returns true otherwise, and fills out_intersection with the intersection
// point.
b32 intersect_line_segments(v2i a, v2i b,
                            v2i u, v2i v,
                            v2f* out_intersection);


// ---------------
// The mighty rect
// ---------------

typedef struct
{
    union {
        struct {
            v2l top_left;
            v2l bot_right;
        };
        struct {
            i64 left;
            i64 top;
            i64 right;
            i64 bottom;
        };
    };
} Rect;

#define VALIDATE_RECT(rect) mlt_assert(rect_is_valid((rect)))

// Splits src_rect into a number of rectangles stored in dest_rects
// Returns the number of rectangles into which src_rect was split.
i32 rect_split(Rect** out_rects, Rect src_rect, i32 width, i32 height);

// Returns a rectangle R such that for any other rectangle B , R union B = B
Rect rect_without_size();

// Set operations on rectangles
Rect rect_union(Rect a, Rect b);
Rect rect_intersect(Rect a, Rect b);
Rect rect_stretch(Rect rect, i32 width);

Rect rect_clip_to_screen(Rect limits, v2i screen_size);

const Rect rect_enlarge(Rect src, i32 offset);

b32 rect_is_valid(Rect rect);

Rect bounding_rect_for_points(v2l points[], i32 num_points);

i32 rect_area(Rect rect);

b32 is_inside_rect(Rect bounds, v2i point);
b32 is_inside_rect_scalar(Rect bounds, i32 point_x, i32 point_y);

b32 is_rect_within_rect(Rect a, Rect b);

Rect rect_from_xywh(i32 x, i32 y, i32 w, i32 h);

wchar_t*    str_trim_to_last_slash(wchar_t* str);
char*       str_trim_to_last_slash(char* str);
void        utf16_to_utf8_simple(char* , char* );
void        utf16_to_utf8_simple(wchar_t* utf16_name, char* utf8_name);


struct Bitmap
{
    i32 width;
    i32 height;
    i32 num_components;
    u8* data;
};

// -----------
// TIME
// -----------

struct WallTime
{
    union
    {
        struct
        {
            i32 hours;
            i32 minutes;
            i32 seconds;
            i32 milliseconds;
        };
        struct
        {
            i32 h;
            i32 m;
            i32 s;
            i32 ms;
        };
    };
};
// Use platform_get_walltime to fill struct.

// Generic swap function
template<typename T>
void
swap(T& a, T& b)
{
    T tmp = a;
    a = b;
    b = tmp;
}


// Hash function

u64 hash(char* string, size_t len);

template<typename T>
T lerp(T begin, T end, float t) {
    return begin * (1.0f - t) + (end * t);
}