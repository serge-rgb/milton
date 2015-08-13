//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


typedef struct v2f_s
{
    union
    {
        struct
        {
            f32 x;
            f32 y;
        };
        struct
        {
            f32 w;
            f32 h;
        };
        f32 d[2];
    };
} v2f;

func v2f sub_v2f (v2f a, v2f b)
{
    v2f result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

func v2f add_v2f (v2f a, v2f b)
{
    v2f result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

func v2f mul_v2f (v2f a, v2f b)
{
    v2f result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    return result;
}

func v2f scale_v2f (v2f v, f32 factor)
{
    v2f result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    return result;
}

func v2f perp_v2f (v2f a)
{
    v2f result =
    {
        -a.y,
        a.x
    };
    return result;
}

func v2f invscale_v2f (v2f v, f32 factor)
{
    v2f result;
    result.x = v.x / factor;
    result.y = v.y / factor;
    return result;
}

typedef struct v3f_s
{
    union
    {
        struct
        {
            f32 x;
            f32 y;
            f32 z;
        };
        struct
        {
            f32 r;
            f32 g;
            f32 b;
        };
        struct
        {
            f32 h;
            f32 s;
            f32 v;
        };
        f32 d[3];
    };
} v3f;

func v3f sub_v3f (v3f a, v3f b)
{
    v3f result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

func v3f add_v3f (v3f a, v3f b)
{
    v3f result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

func v3f mul_v3f (v3f a, v3f b)
{
    v3f result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    return result;
}

func v3f scale_v3f (v3f v, f32 factor)
{
    v3f result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    result.z = factor * v.z;
    return result;
}

func v3f invscale_v3f (v3f v, f32 factor)
{
    v3f result;
    result.x = v.x / factor;
    result.y = v.y / factor;
    result.z = v.z / factor;
    return result;
}

typedef struct v4f_s
{
    union
    {
        struct
        {
            f32 x;
            f32 y;
            f32 z;
            f32 w;
        };
        struct
        {
            f32 r;
            f32 g;
            f32 b;
            f32 a;
        };
        union
        {
            v3f rgb;
            f32  _pad__a;
        };
        f32 d[4];
    };
} v4f;

typedef struct v2i_s
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

func v2i sub_v2i (v2i a, v2i b)
{
    v2i result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

func v2i add_v2i (v2i a, v2i b)
{
    v2i result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

func v2i mul_v2i (v2i a, v2i b)
{
    v2i result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    return result;
}

func v2i scale_v2i (v2i v, i32 factor)
{
    v2i result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    return result;
}

func v2i perp_v2i (v2i a)
{
    v2i result =
    {
        -a.y,
        a.x
    };
    return result;
}

func v2i invscale_v2i (v2i v, i32 factor)
{
    v2i result;
    result.x = v.x / factor;
    result.y = v.y / factor;
    return result;
}

typedef struct v3i_s
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

func v3i sub_v3i (v3i a, v3i b)
{
    v3i result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

func v3i add_v3i (v3i a, v3i b)
{
    v3i result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

func v3i mul_v3i (v3i a, v3i b)
{
    v3i result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    return result;
}

func v3i scale_v3i (v3i v, i32 factor)
{
    v3i result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    result.z = factor * v.z;
    return result;
}

func v3i invscale_v3i (v3i v, i32 factor)
{
    v3i result;
    result.x = v.x / factor;
    result.y = v.y / factor;
    result.z = v.z / factor;
    return result;
}

typedef struct v4i_s
{
    union
    {
        struct
        {
            i32 x;
            i32 y;
            i32 z;
            i32 w;
        };
        struct
        {
            i32 r;
            i32 g;
            i32 b;
            i32 a;
        };
        union
        {
            v3i rgb;
            i32  _pad__a;
        };
        i32 d[4];
    };
} v4i;

typedef struct v2l_s
{
    union
    {
        struct
        {
            i64 x;
            i64 y;
        };
        struct
        {
            i64 w;
            i64 h;
        };
        i64 d[2];
    };
} v2l;

func v2l sub_v2l (v2l a, v2l b)
{
    v2l result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

func v2l add_v2l (v2l a, v2l b)
{
    v2l result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

func v2l mul_v2l (v2l a, v2l b)
{
    v2l result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    return result;
}

func v2l scale_v2l (v2l v, i64 factor)
{
    v2l result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    return result;
}

func v2l perp_v2l (v2l a)
{
    v2l result =
    {
        -a.y,
        a.x
    };
    return result;
}

func v2l invscale_v2l (v2l v, i64 factor)
{
    v2l result;
    result.x = v.x / factor;
    result.y = v.y / factor;
    return result;
}

typedef struct v3l_s
{
    union
    {
        struct
        {
            i64 x;
            i64 y;
            i64 z;
        };
        struct
        {
            i64 r;
            i64 g;
            i64 b;
        };
        struct
        {
            i64 h;
            i64 s;
            i64 v;
        };
        i64 d[3];
    };
} v3l;

func v3l sub_v3l (v3l a, v3l b)
{
    v3l result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

func v3l add_v3l (v3l a, v3l b)
{
    v3l result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

func v3l mul_v3l (v3l a, v3l b)
{
    v3l result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    return result;
}

func v3l scale_v3l (v3l v, i64 factor)
{
    v3l result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    result.z = factor * v.z;
    return result;
}

func v3l invscale_v3l (v3l v, i64 factor)
{
    v3l result;
    result.x = v.x / factor;
    result.y = v.y / factor;
    result.z = v.z / factor;
    return result;
}

typedef struct v4l_s
{
    union
    {
        struct
        {
            i64 x;
            i64 y;
            i64 z;
            i64 w;
        };
        struct
        {
            i64 r;
            i64 g;
            i64 b;
            i64 a;
        };
        union
        {
            v3l rgb;
            i64  _pad__a;
        };
        i64 d[4];
    };
} v4l;

