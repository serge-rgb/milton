// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "common.h"
#include "vector.h"


v2f
lerp(v2f a, v2f b, float t)
{
    return b*t + a*(1.0f-t);
}
