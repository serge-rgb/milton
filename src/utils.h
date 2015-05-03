// utils.h
// (c) Copyright 2015 by Sergio Gonzalez.

// -------------
// Useful macros
// -------------

#define stack_count(arr) (sizeof((arr)) / sizeof((arr)[0]))

// -----------
// System stuf
// -----------

typedef struct BitScanResult_s
{
    uint32 index;
    bool32 found;
} BitScanResult;

inline BitScanResult find_least_significant_set_bit(uint32 value)
{
    BitScanResult result = { 0 };
#if defined(_MSC_VER)
    result.found = _BitScanForward((DWORD*)&result.index, value);
#else
    for (uint32 i = 0; i < 32; ++i)
    {
        if (value & (1 << i))
        {
            result.index = i;
            result.found = true;
            break;
        }
    }
#endif
    return result;
}

// ---------------
// The mighty rect
// ---------------

typedef struct Rect_s
{
    union
    {
        struct
        {
            v2i top_left;
            v2i bot_right;
        };
        struct
        {
            int32 left;
            int32 top;
            int32 right;
            int32 bottom;
        };
    };
} Rect;

// Returns the number of rectangles into which src_rect was split.
static int32 rect_split(Arena* transient_arena,
        Rect src_rect, int32 width, int32 height, Rect** dest_rects)
{
    int n_width = (src_rect.right - src_rect.left) / width;
    int n_height = (src_rect.bottom - src_rect.top) / height;

    if (!n_width || !n_height)
    {
        return 0;
    }

    int32 max_num_rects = (n_width + 1) * (n_height + 1);
    *dest_rects = arena_alloc_array(transient_arena, max_num_rects, Rect);

    int32 i = 0;
    for (int h = src_rect.top; h < src_rect.bottom; h += height)
    {
        for (int w = src_rect.left; w < src_rect.right; w += width)
        {
            Rect rect;
            {
                rect.left = w;
                rect.right = min(src_rect.right, w + width);
                rect.top = h;
                rect.bottom = min(src_rect.bottom, h + height);
            }
            (*dest_rects)[i++] = rect;
        }
    }
    assert(i <= max_num_rects);
    return i;
}

inline Rect rect_clip_to_screen(Rect limits, v2i screen_size)
{
    if (limits.left < 0) limits.left = 0;
    if (limits.right > screen_size.w) limits.right = screen_size.w;
    if (limits.top < 0) limits.top = 0;
    if (limits.bottom > screen_size.h) limits.bottom = screen_size.h;
    return limits;
}

inline Rect rect_enlarge(Rect src, int32 offset)
{
    Rect result;
    result.left = src.left - offset;
    result.top = src.top - offset;
    result.right = src.right + offset;
    result.bottom = src.bottom + offset;
    return result;
}

static Rect bounding_rect_for_points(v2i points[], int64 num_points)
{
    assert (num_points > 0);

    v2i top_left =  points[0];
    v2i bot_right = points[0];

    for (int64 i = 1; i < num_points; ++i)
    {
        v2i point = points[i];
        if (point.x < top_left.x)   top_left.x = point.x;
        if (point.x > bot_right.x)  bot_right.x = point.x;

        if (point.y < top_left.y)   top_left.y = point.y;
        if (point.y > bot_right.y)  bot_right.y = point.y;
    }
    Rect rect = { top_left, bot_right };
    return rect;
}

inline int32 rect_area(Rect rect)
{
    return (rect.right - rect.left) * (rect.bottom - rect.top);
}

inline bool32 is_inside_rect(v2i point, Rect bounds)
{
    return
        point.x >= bounds.left &&
        point.x <  bounds.right &&
        point.y >= bounds.top &&
        point.y <  bounds.bottom;
}

inline v2i v2f_to_v2i(v2f p)
{
    return (v2i){(int32)p.x, (int32)p.y};
}

inline v2f v2i_to_v2f(v2i p)
{
    return (v2f){(float)p.x, (float)p.y};
}

inline v3f v4f_to_v3f(v4f v)
{
    return (v3f){v.r, v.g, v.b};
}

// ---------------
// Math functions.
// ---------------

#define kPi 3.14152654f


inline float absf(float a)
{
    return a < 0 ? -a : a;
}

inline int32 absi(int32 a)
{
    return a < 0 ? -a : a;
}

inline float dot(v2f a, v2f b)
{
   return a.x * b.x + a.y * b.y;
}

inline float magnitude(v2f a)
{
    return sqrtf(dot(a, a));
}

inline float radians_to_degrees(float r)
{
    return (180 * r) / kPi;
}

