// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "common.h"
#include "math.h"


v2f
lerp(v2f a, v2f b, float t)
{
    return b*t + a*(1.0f-t);
}

Vector2<i32>
VEC2I(Vector2<i64> o)
{
    Vector2<i32> r = {
        (i32)o.x,
        (i32)o.y,
    };
    return r;
}

Vector2<i64>
VEC2L(Vector2<i32> o)
{
    Vector2<i64> r = {
        (i64)o.x,
        (i64)o.y,
    };
    return r;
}
