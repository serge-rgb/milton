// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.


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
    v3f result;
    result.x *= factor;
    result.y *= factor;
    result.z *= factor;
    return result;
}

v3i scale3i(v3i a, i32 factor)
{
    v3i result;
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

