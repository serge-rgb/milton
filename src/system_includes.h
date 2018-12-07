// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include <imgui.h>


#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(push, 0)
#endif  // _WIN32 && _MSC_VER

#if defined(__clang__)
#pragma clang system_header
#endif

#define GetWindowFont _GetWindowFont

#ifdef _WIN32
/* #define VC_EXTRALEAN */
/* #define WIN32_LEAN_AND_MEAN */
#include <windows.h>
#include <windowsx.h>
#define GCL_HICON -14
#endif


// SDL
#include <SDL.h>
#include <SDL_syswm.h>

// Platform independent includes:
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <xmmintrin.h>
#include <emmintrin.h>

#if defined(_WIN32)

#include "gl.h"

#elif defined(__linux__)

#include "gl.h"

// #define GL_GLEXT_PROTOTYPES
// #include <GL/gl.h>
// #include <GL/glext.h>

#include <dlfcn.h>  // Dynamic library loading.

#elif defined (__MACH__)

// #define GL_GLEXT_PROTOTYPES 1
// #include "SDL_opengl.h"
#include "gl.h"

#endif // OpenGL includes

#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(pop)
#endif  // _WIN32 && _MSC_VER

