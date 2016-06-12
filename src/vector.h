// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

template<typename T>
struct Vector2
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
        T xy[2];
    };

    // Constructors...

    Vector2 ()
    {
        x = 0;
        y = 0;
    }
    Vector2 (const T& val)
    {
        x = val;
        y = val;
    }
    Vector2 (const Vector2<T>& other)
    {
        x = other.x;
        y = other.y;
    }

    Vector2<T> operator =(const Vector2<T>& other)
    {
        this->x = other.x;
        this->y = other.y;
        return *this;
    }

    /* Vector2 (Vector2<float> other) */
    /* { */
    /*     x = (T)other.x; */
    /*     y = (T)other.y; */
    /* } */

    // Specialized Assignment

    /* Vector2<i32> operator =(const Vector2<float>& a) */
    /* { */
    /*     ivec2 result; */
    /*     result.x = (int)a.x; */
    /*     result.y = (int)a.y; */
    /*     return result; */
    /* } */

    /* Vector2<float> operator =(const Vector2<i32>&& a) */
    /* { */
    /*     ivec2 result; */
    /*     result.x = (int)a.x; */
    /*     result.y = (int)a.y; */
    /*     return result; */
    /* } */

};

// Types
typedef Vector2<int>     ivec2;
typedef Vector2<float>   vec2;

template<typename T>
b32 operator ==(const Vector2<T>& a, const Vector2<T>& b)
{
    b32 result = a.x == b.x && a.y == b.y;
    return result;
}
template<typename T>
Vector2<T> operator +(const Vector2<T>& a, const Vector2<T>& b)
{
    Vector2<T> result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}
Vector2<float> operator +(const Vector2<float>& a, const Vector2<i32>& b)
{
    Vector2<float> result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}
template<typename T>
Vector2<T> operator -(const Vector2<T>& a, const Vector2<T>& b)
{

    Vector2<T> result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}
template<typename T>
Vector2<T> operator -=(Vector2<T>& a, const Vector2<T>& b)
{
    a.x -= b.x;
    a.y -= b.y;
    return a;
}
template<typename T>
Vector2<T> operator -=(Vector2<T>& a, double f)
{
    a.x -= f;
    a.y -= f;
    return a;
}
template<typename T>
Vector2<T> operator -=(Vector2<T>& a, float f)
{
    a.x -= f;
    a.y -= f;
    return a;
}
template<typename T>
Vector2<T> operator -=(Vector2<T>& a, i32 f)
{
    a.x -= f;
    a.y -= f;
    return a;
}
template<typename T>
Vector2<T> operator *(const Vector2<T>& a, T factor)
{
    Vector2<T> result = a;
    result.x *= factor;
    result.y *= factor;
    return result;
}
template<typename T>
Vector2<T> operator *=(Vector2<T>& a, double f)
{
    a.x *= f;
    a.y *= f;
    return a;
}
template<typename T>
Vector2<T> operator *=(Vector2<T>& a, float f)
{
    a.x *= f;
    a.y *= f;
    return a;
}
template<typename T>
Vector2<T> operator *=(Vector2<T>& a, i32 f)
{
    a.x *= f;
    a.y *= f;
    return a;
}
template<typename T>
Vector2<T> operator /(Vector2<T>& a, Vector2<T>& b)
{
    Vector2<T> result = a;
    result.x /= b.x;
    result.y /= b.y;
    return result;
}
template<typename T>
Vector2<T> operator /=(Vector2<T>& a, const Vector2<T>& b)
{
    a.x /= b.x;
    a.y /= b.y;
    return a;
}
template<typename T>
Vector2<T> operator /(const Vector2<T>& a, T factor)
{
    Vector2<T> result = a;
    result.x /= factor;
    result.y /= factor;
    return result;
}

template<typename T>
Vector2<T> _perpendicular (const Vector2<T>& a)
{
    Vector2<T> result =
    {
        -a.y,
        a.x
    };
    return result;
}

typedef struct
{
    union
    {
        struct
        {
            float x;
            float y;
        };
        struct
        {
            float w;
            float h;
        };
        float d[2];
    };
} v2f;

typedef struct
{
    union
    {
        struct
        {
            i32 x;
            i32 y;
        };
        struct
        {
            i32 w;
            i32 h;
        };
        i32 d[2];
    };
} v2i;


b32 equ2f(v2f a, v2f b);

b32 equ2i(v2i a, v2i b);

v2f sub2f(v2f a, v2f b);

v2i sub2i(v2i a, v2i b);

v2f add2f(v2f a, v2f b);

v2i add2i(v2i a, v2i b);

v2f scale2f(v2f a, float factor);

v2i scale2i(v2i a, i32 factor);

v2i divide2i(v2i a, i32 factor);

v2i perpendicular (v2i a);

typedef struct
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
        };
        struct
        {
            float r;
            float g;
            float b;
        };
        struct
        {
            float h;
            float s;
            float v;
        };
        float d[3];
    };
} v3f;

typedef struct
{
    union
    {
        struct
        {
            i32 x;
            i32 y;
            i32 z;
        };
        struct
        {
            i32 r;
            i32 g;
            i32 b;
        };
        struct
        {
            i32 h;
            i32 s;
            i32 v;
        };
        i32 d[3];
    };
} v3i;


b32 equ3f(v3f a, v3f b);

b32 equ3i(v3i a, v3i b);

v3f scale3f(v3f a, float factor);

v3i scale3i(v3i a, i32 factor);

typedef struct
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
        struct
        {
            float r;
            float g;
            float b;
            float a;
        };
        union
        {
            v3f rgb;
            float  pad__a;
        };
        union
        {
            v3f xyz;
            float  pad__w;
        };
        float d[4];
    };
} v4f;

b32 equ4f(v4f a, v4f b);

