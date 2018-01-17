// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#ifndef MLT_COMMON
#define MLT_COMMON

#include "milton_configuration.h"

#include <stdint.h>
#include <stddef.h>

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

#ifdef ALIGN
    #error ALIGN macro already defined.
#else
    #if defined(_MSC_VER)
        #define ALIGN(n) __declspec(align(n))
    #elif defined(__GNUC__)  // Clang defines __GNUC__ too
        #define ALIGN(n) __attribute__(( aligned (n) ))
    #else
        #error I dont know how to align stuff in this compiler
    #endif
#endif // ALIGN

// Assert implementation

#if defined(mlt_assert)
#error mlt_assert already defined
#else
    #if defined(_WIN32)
    #define mlt_assert(expr)  do { if (!(bool)(expr)) {  \
                                    MessageBox(NULL,"Assertion: " #expr "-" __FILE__, "Assertion", MB_OK);\
                                     __debugbreak(); \
                                } } while(0)
    #define mlt_assert_without_msgbox(expr)  do { if (!(bool)(expr)) {  \
                                     __debugbreak(); \
                                } } while(0)

    #elif defined(__MACH__)
    #define mlt_assert(expr)  do { if (!(bool)(expr)) {  __builtin_trap();  } } while(0)
    #define mlt_assert_without_msgbox(expr)  do { if (!(bool)(expr)) {  __builtin_trap();  } } while(0)
    #else
    #define mlt_assert(expr)  do { if (!(bool)(expr)) {  (*(u32*)0) = 0xDeAdBeEf;  } } while(0)
    #define mlt_assert_without_msgbox(expr)  do { if (!(bool)(expr)) {  (*(u32*)0) = 0xDeAdBeEf;  } } while(0)
    #endif
#endif

#ifndef MLT_ABS
    #define MLT_ABS(x) (((x) < 0) ? -(x) : (x))
#endif


#define INVALID_CODE_PATH mlt_assert(!"Invalid code path")

#if defined(MILTON_DEBUG)
    #if defined(_WIN32)
        #define BREAKHERE __debugbreak()
    #endif
    #if defined(__MACH__)
        #define BREAKHERE asm ("int $3")
    #endif
#endif

#endif  // MLT_COMMON
