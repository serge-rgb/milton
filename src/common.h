// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include <stdint.h>

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef float       f32;

typedef i32         b32;

#if defined(_WIN32)
#define PATH_CHAR wchar_t
#define TO_PATH_STR(STR) L##STR
#else
#define PATH_CHAR char
#define TO_PATH_STR(STR) STR
#endif


#define type(name) \
        typedef struct name name; \
        struct name

#include "system_includes.h"

#ifdef UNUSED
#error "Someone else defined UNUSED macro"
#else
#define UNUSED(x) (void*)(&x)
#endif

#ifdef ALIGN
#error ALIGN macro already defined.
#else
#if defined(_MSC_VER)
#define ALIGN(n) __declspec(align(n))
#elif defined(__GNUC__)  // Clang defines this too
#define ALIGN(n) __attribute__(( aligned (n) ))
#else
#error I don't know how to align stuff in this compiler
#endif
#endif // ALIGN

// Assert implementation

#if defined(assert)
//#error assert already defined
#else
#define assert(expr)  do { if (!(bool)(expr)) {  (*(u32*)0) = 0xDeAdBeEf;  } } while(0)
#endif

#define INVALID_CODE_PATH assert(!"Invalid code path")

