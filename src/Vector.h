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

#pragma once

template<typename T>
struct Vec2
{
    union
    {
        struct
        {
            T x;
            T y;
        };
        struct
        {
            T w;
            T h;
        };
        T d[2];
    };
};

typedef Vec2<f32> v2f;
typedef Vec2<i32> v2i;

template<typename T> bool operator== (Vec2<T> a, Vec2<T> b)
{
    bool result = a.x == b.x && a.y == b.y;
    return result;
}

template<typename T> bool operator!= (Vec2<T> a, Vec2<T> b)
{
    bool result = !(a == b);
    return result;
}

template<typename T> Vec2<T> operator- (Vec2<T> a, Vec2<T> b)
{

    Vec2<T> result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

template<typename T> Vec2<T> operator+ (Vec2<T> a, Vec2<T> b)
{
    Vec2<T> result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

template<typename T> Vec2<T> operator* (Vec2<T> a, Vec2<T> b)
{
    Vec2<T> result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    return result;
}

template<typename T> Vec2<T> operator* (Vec2<T> v, T factor)
{
    Vec2<T> result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    return result;
}

template<typename T> Vec2<T> operator/ (Vec2<T> v, T factor)
{
    Vec2<T> result;
    result.x = v.x / factor;
    result.y = v.y / factor;
    return result;
}

template<typename T> Vec2<T> perpendicular (Vec2<T> a)
{
    Vec2<T> result =
    {
        -a.y,
        a.x
    };
    return result;
}

template<typename T>
struct Vec3
{
    union
    {
        struct
        {
            T x;
            T y;
            T z;
        };
        struct
        {
            T r;
            T g;
            T b;
        };
        struct
        {
            T h;
            T s;
            T v;
        };
        T d[3];
    };
};

typedef Vec3<f32> v3f;

template<typename T> bool operator== (Vec3<T> a, Vec3<T> b)
{
    bool result = a.x == b.x && a.y == b.y && a.z == b.z;
    return result;
}

template<typename T> Vec3<T> operator* (Vec3<T> v, T factor)
{
    Vec3<T> result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    result.z = factor * v.z;
    return result;
}

/* template<typename T> Vec2<T> operator*= (Vec2<T> v, T factor) */
/* { */
/*     Vec2<T> result; */
/*     result.x = factor * v.x; */
/*     result.y = factor * v.y; */
/*     result.z = factor * v.z; */
/*     return result; */
/* } */

template<typename T>
struct Vec4
{
    union
    {
        struct
        {
            T x;
            T y;
            T z;
            T w;
        };
        struct
        {
            T r;
            T g;
            T b;
            T a;
        };
        union
        {
            Vec3<T> rgb;
            T  pad__a;
        };
        union
        {
            Vec3<T> xyz;
            T  pad__w;
        };
        T d[4];
    };
};

typedef Vec4<f32> v4f;

template<typename T> bool operator== (Vec4<T> a, Vec4<T> b)
{
    bool result = a.x == b.x && a.y == b.y && a.z == b.z && a.a == b.a;
    return result;
}
