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
};

// Types
typedef Vector2<int>     ivec2;
typedef Vector2<float>   vec2;

#pragma warning(push)
#pragma warning(disable:4587) // Constructor for xyz not implicitly called
template<typename T>
struct Vector3
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
            Vector2<T> xy;
            T z_;
        };
        T d[3];
        T xyz[3];
    };

    // Constructors...

    Vector3 ()
    {
        x = 0;
        y = 0;
        z = 0;
    }
    Vector3 (const T& val)
    {
        x = val;
        y = val;
        z = val;
    }
    Vector3 (const Vector3<T>& other)
    {
        x = other.x;
        y = other.y;
        z = other.z;
    }

    Vector3<T> operator =(const Vector3<T>& other)
    {
        this->x = other.x;
        this->y = other.y;
        this->z = other.z;
        return *this;
    }
};
#pragma warning(pop)

// Types
typedef Vector3<int>     ivec3;
typedef Vector3<float>   vec3;

#define op3_T(OP) \
        template<typename T> \
        Vector3<T> operator OP (const Vector3<T>& v, T f) \
{\
    \
    Vector3<T> r = v; \
    r.x  OP= f; \
    r.y  OP= f; \
    r.z  OP= f; \
    return v; \
}
#define op3(OP) \
        template<typename T> \
        Vector3<T> operator OP (const Vector3<T>& v, const Vector3<T>& o) \
{\
    \
    Vector3<T> r = v; \
    r.x  OP= o.x; \
    r.y  OP= o.y; \
    r.z  OP= o.z; \
    return v; \
}
#define op3_F(OP, TYPE) \
        template<typename T> \
        Vector3<T> operator OP (Vector3<T>&v, TYPE f) \
{ \
    v.x  OP f; \
    v.y  OP f; \
    v.z  OP f; \
    return v;\
}
#define op3_M(OP) \
        template<typename T> \
        Vector3<T> operator OP (Vector3<T>&v, Vector3<T>& o) \
{ \
    v.x  OP o.x; \
    v.y  OP o.y; \
    v.z  OP o.z; \
    return v;\
}


op3_T(*)
/* op3_T(+) */
op3_F(*=, i32)
op3_F(/=, i32)
op3_M(+=)

/* op3(+) */

#pragma warning(push)
#pragma warning(disable:4587) // Constructor for xyz not implicitly called
template<typename T>
struct Vector4
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
        T d[4];
        struct
        {

            Vector3<T> xyz;
            float w_;
        };
        struct
        {
            Vector2<T> xy;
            Vector2<T> zw;
        };
        struct
        {

            Vector3<T> rgb;
            float a_;
        };

        T xyzw[4];
        T rgba[4];
    };

    // Constructors...

    Vector4 ()
    {
        x = 0;
        y = 0;
        z = 0;
        w = 0;
    }
    Vector4 (const T& val)
    {
        x = val;
        y = val;
        z = val;
        w = val;
    }
    Vector4 (const Vector4<T>& other)
    {
        x = other.x;
        y = other.y;
        z = other.z;
        w = other.w;
    }

    Vector4<T> operator =(const Vector4<T>& other)
    {
        this->x = other.x;
        this->y = other.y;
        this->z = other.z;
        this->w = other.w;
        return *this;
    }
};
#pragma warning(pop)
#define op4_T(OP) \
        template<typename T> \
        Vector4<T> operator OP (const Vector4<T>& v, T f) \
{\
    \
    Vector4<T> r = v; \
    r.x  OP= f; \
    r.y  OP= f; \
    r.z  OP= f; \
    r.w  OP= f; \
    return v; \
}
#define op4(OP) \
        template<typename T> \
        Vector4<T> operator OP (const Vector4<T>& v, const Vector4<T>& o) \
{\
    \
    Vector4<T> r = v; \
    r.x  OP= o.x; \
    r.y  OP= o.y; \
    r.z  OP= o.z; \
    r.w  OP= o.w; \
    return v; \
}
#define op4_M(OP, TYPE) \
        template<typename T> \
        Vector4<T> operator OP (Vector4<T>&v, TYPE f) \
{ \
    v.x  OP f; \
    v.y  OP f; \
    v.z  OP f; \
    v.w  OP f; \
    return v;\
}

op4_T(*)
op4_T(+)
//op4_M(*=, i32)

op4(+)


// Types
typedef Vector4<float>   vec4;

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

