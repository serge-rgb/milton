// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "common.h"

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

