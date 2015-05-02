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

// -----------
// POD helpers
// -----------

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

