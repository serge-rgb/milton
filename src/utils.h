// utils.h
// (c) Copyright 2015 by Sergio Gonzalez.

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

inline v2i v2f_to_v2i(v2f p)
{
    return (v2i){(int32)p.x, (int32)p.y};
}
inline v2f v2i_to_v2f(v2i p)
{
    return (v2f){(float)p.x, (float)p.y};
}

// ---------------
// Math functions.
// ---------------

inline float absf(float a)
{
    return a < 0 ? -a : a;
}

inline int32 absi(int32 a)
{
    return a < 0 ? -a : a;
}

inline int32 maxi(int32 a, int32 b)
{
    return a > b? a : b;
}
inline int32 mini(int32 a, int32 b)
{
    return a < b? a : b;
}

inline float maxf(float a, float b)
{
    return a > b? a : b;
}

inline float minf(float a, float b)
{
    return a < b? a : b;
}

inline int64 minl(int64 a, int64 b)
{
    return a < b? a : b;
}

