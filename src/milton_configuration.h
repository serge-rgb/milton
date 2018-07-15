// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#define MILTON_MAJOR_VERSION 1
#define MILTON_MINOR_VERSION 6
#define MILTON_MICRO_VERSION 2

#if !defined(MILTON_DEBUG)
    #define MILTON_DEBUG 1
#endif


#define MILTON_ZOOM_DEBUG 0
    // Force MILTON_ZOOM_DEBUG to 0 if MILTON_DEBUG is 0.
#if !MILTON_DEBUG
    #undef MILTON_ZOOM_DEBUG
    #define MILTON_ZOOM_DEBUG 0
#endif

// Windows Options
#if defined(_WIN32)
    // If 1, print to VS console. Debug messages always print to log file.
    #define WIN32_DEBUGGER_OUTPUT 1
    #if !MILTON_DEBUG
        #undef WIN32_DEBUGGER_OUTPUT
        #define WIN32_DEBUGGER_OUTPUT 0
    #endif
#endif

#define MULTISAMPLING_ENABLED 0  // When disabled, the renderer uses FXAA for screen-space AA.
    #define MSAA_NUM_SAMPLES 4

#define MILTON_MULTITHREADED 1

#define MILTON_ENABLE_PROFILING 1

#define REDRAW_EVERY_FRAME 1

#define STROKE_DEBUG_VIZ 0
  #if !MILTON_DEBUG
    #undef STROKE_DEBUG_VIZ
    #define STROKE_DEBUG_VIZ 0
  #endif


#define MILTON_HARDWARE_BRUSH_CURSOR 1
 // No support for system cursor on linux or macos for now
#if defined(__linux__) || defined(__MACH__)
#undef MILTON_HARDWARE_BRUSH_CURSOR
#define MILTON_HARDWARE_BRUSH_CURSOR 0
#endif

// Uses GL 2.1 when 0
#define USE_GL_3_2 1
    #if !MILTON_DEBUG  // Don't use 3.2 in release.
        #undef USE_GL_3_2
        #define USE_GL_3_2 0
    #endif

    // Use 3.2 on macos. OpenGL 3.2 is supported by all mac computers since macOS 10.8.5
    // https://developer.apple.com/opengl/OpenGL-Capabilities-Tables.pdf
    #if defined(__MACH__)
        #undef USE_GL_3_2
        #define USE_GL_3_2 1
    #endif

#define DEBUG_MEMORY_USAGE 0

// Spawn threads to save the canvas.
#define MILTON_SAVE_ASYNC 1

#ifdef CMAKE_TRY_GL2
    #undef USE_GL_3_2
    #define USE_GL_3_2 0
#endif

#ifdef CMAKE_TRY_GL3
    #undef USE_GL_3_2
    #define USE_GL_3_2 1
#endif
