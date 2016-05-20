// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "vector.h"

b32 equ2f(v2f a, v2f b)
{
    b32 result = a.x == b.x && a.y == b.y;
    return result;
}

b32 equ2i(v2i a, v2i b)
{
    b32 result = a.x == b.x && a.y == b.y;
    return result;
}

v2f sub2f(v2f a, v2f b)
{
    v2f result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

v2i sub2i(v2i a, v2i b)
{

    v2i result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

v2f add2f(v2f a, v2f b)
{
    v2f result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

v2i add2i(v2i a, v2i b)
{
    v2i result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

v2f scale2f(v2f a, float factor)
{
    v2f result = a;
    result.x *= factor;
    result.y *= factor;
    return result;
}

v2i scale2i(v2i a, i32 factor)
{
    v2i result = a;
    result.x *= factor;
    result.y *= factor;
    return result;
}

v2i divide2i(v2i a, i32 factor)
{
    v2i result = a;
    result.x /= factor;
    result.y /= factor;
    return result;
}

v2i perpendicular (v2i a)
{
    v2i result =
    {
        -a.y,
        a.x
    };
    return result;
}

b32 equ3f(v3f a, v3f b)
{
    bool result = a.x == b.x && a.y == b.y && a.z == b.z;
    return result;
}

b32 equ3i(v3i a, v3i b)
{
    bool result = a.x == b.x && a.y == b.y && a.z == b.z;
    return result;
}

v3f scale3f(v3f a, float factor)
{
    v3f result = a;
    result.x *= factor;
    result.y *= factor;
    result.z *= factor;
    return result;
}

v3i scale3i(v3i a, i32 factor)
{
    v3i result = a;
    result.x *= factor;
    result.y *= factor;
    result.z *= factor;
    return result;
}

b32 equ4f(v4f a, v4f b)
{
    bool result = a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    return result;
}

