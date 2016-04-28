// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "vector.h"

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

// -----------
// System stuf
// -----------

// total RAM in bytes
size_t get_system_RAM();

// ---------------
// Math functions.
// ---------------

v2i v2f_to_v2i(v2f p);

v2f v2i_to_v2f(v2i p);


#define kPi 3.14152654f

#define DOT(a, b)  ((a).x * (b).x + (a).y * (b).y)

f32 magnitude(v2f a);

f32 distance(v2f a, v2f b);

i32 manhattan_distance(v2i a, v2i b);

f32 deegrees_to_radians(int d);

f32 radians_to_degrees(f32 r);

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

typedef struct {
    union {
        struct {
            v2i top_left;
            v2i bot_right;
        };
        struct {
            i32 left;
            i32 top;
            i32 right;
            i32 bottom;
        };
    };
} Rect;

#define VALIDATE_RECT(rect) assert(rect_is_valid((rect)))

// Splits src_rect into a number of rectangles stored in dest_rects
// Returns the number of rectangles into which src_rect was split.
i32 rect_split(Rect** out_rects, Rect src_rect, i32 width, i32 height);

// Set operations on rectangles
Rect rect_union(Rect a, Rect b);
Rect rect_intersect(Rect a, Rect b);
Rect rect_stretch(Rect rect, i32 width);

Rect rect_clip_to_screen(Rect limits, v2i screen_size);

const Rect rect_enlarge(Rect src, i32 offset);

b32 rect_is_valid(Rect rect);

Rect bounding_rect_for_points(v2i points[], i32 num_points);
Rect bounding_rect_for_points_scalar(i32 points_x[], i32 points_y[], i32 num_points);

i32 rect_area(Rect rect);

b32 is_inside_rect(Rect bounds, v2i point);
b32 is_inside_rect_scalar(Rect bounds, i32 point_x, i32 point_y);

b32 is_rect_within_rect(Rect a, Rect b);

Rect rect_from_xywh(i32 x, i32 y, i32 w, i32 h);

typedef struct
{
    i32 width;
    i32 height;
    i32 num_components;
    u8* data;
} Bitmap;


// STB stretchy buffer

#define sb_free(a)          ((a) ? free(sb__sbraw(a)),0 : 0)
#define sb_push(a,v)        (sb__sbmaybegrow(a,1), (a)[sb__sbn(a)++] = (v))
#define sb_reset(a)         ((a) ? (sb__sbn(a) = 0) : 0)
#define sb_pop(a)           ((a)[--sb__sbn(a)])
#define sb_unpop(a)         (sb__sbn(a)++)
#define sb_count(a)         ((a) ? sb__sbn(a) : 0)
#define sb_reserve(a,n)     (sb__sbmaybegrow(a,n), sb__sbn(a)+=(n), &(a)[sb__sbn(a)-(n)])
#define sb_peek(a)          ((a)[sb__sbn(a)-1])

#define sb__sbraw(a) ((i32 *) (a) - 2)
#define sb__sbm(a)   sb__sbraw(a)[0]
#define sb__sbn(a)   sb__sbraw(a)[1]

#define sb__sbneedgrow(a,n)  ((a)==0 || sb__sbn(a)+(n) >= sb__sbm(a))
#define sb__sbmaybegrow(a,n) (sb__sbneedgrow(a,(n)) ? sb__sbgrow(a,n) : 0)
#define sb__sbgrow(a,n)      ((a) = sb__sbgrowf((a), (n), sizeof(*(a))))

#define sb_delete_at_idx(a, i) \
        do {\
            if (sb_count(a)) for (int _i=i;_i<sb_count((a))-1;++_i) {\
                (a)[i]=(a)[i+1];\
            }\
            sb_pop(a);\
        } while(0)

void* sb__sbgrowf(void *arr, int increment, int itemsize);

// ASCII String utils

// Returns a stretchy buffer of malloc'ed strings.
char** str_tokenize(char* in);

// Takes a stretchy buffer returned from str_* util functions.
void str_free(char** strings);

// Returns a pointer in the same string to the first char that does not have any (back)slashes in front.
char* str_trim_to_last_slash(char* str);

// -----------
// TIME
// -----------

typedef struct WallTime
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
} WallTime;
// Use platform_get_walltime to fill struct.


