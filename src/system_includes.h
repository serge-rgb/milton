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


#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(push, 0)
#endif  // _WIN32 && _MSC_VER

#if defined(__clang__)
#pragma clang system_header
#endif

#ifdef _WIN32
/* #define VC_EXTRALEAN */
/* #define WIN32_LEAN_AND_MEAN */
#include <windows.h>
#include <windowsx.h>
#endif

// SDL
#include <SDL.h>
#include <SDL_syswm.h>

// Platform independent includes:
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <xmmintrin.h>
#include <emmintrin.h>

#if defined(_WIN32)

#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/wglew.h>

#elif defined(__linux__)

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#elif defined (__MACH__)

#define GL_GLEXT_PROTOTYPES 1
#include "SDL_opengl.h"

#endif // defined(platform)

#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(pop)
#endif  // _WIN32 && _MSC_VER

