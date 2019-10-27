// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#define MILTON_MAJOR_VERSION 1
#define MILTON_MINOR_VERSION 9
#define MILTON_MICRO_VERSION 1


#if !defined(MILTON_DEBUG)  // Might be defined by cmake
    #define MILTON_DEBUG 0
#endif


// Debug settings

#define MILTON_ENABLE_PROFILING 0
#define REDRAW_EVERY_FRAME 0
#define GRAPHICS_DEBUG 0
#define MILTON_ZOOM_DEBUG 0
#define STROKE_DEBUG_VIZ 0
#define DEBUG_MEMORY_USAGE 0
// Windows Debug Options
#if defined(_WIN32)
    // If 1, print to VS console. Debug messages always print to log file.
    #define WIN32_DEBUGGER_OUTPUT 1
#endif


#define MILTON_MULTITHREADED 1

#define MILTON_HARDWARE_BRUSH_CURSOR 1


// Zoom control
#define MINIMUM_SCALE        (1 << 4)

#define SCALE_FACTOR 1.3f
    #if MILTON_ZOOM_DEBUG
        #undef SCALE_FACTOR
        #define SCALE_FACTOR 1.5f
    #endif

#define VIEW_SCALE_LIMIT (1 << 16)
    #if MILTON_ZOOM_DEBUG
        #undef VIEW_SCALE_LIMIT
        #define VIEW_SCALE_LIMIT (1 << 20)
    #endif

#define DEFAULT_PEEK_OUT_INCREMENT_LOG 2.0

#define PEEK_OUT_SPEED 20  // ms / increment

// No support for system cursor on linux or macos for now
#if defined(__linux__) || defined(__MACH__)
#undef MILTON_HARDWARE_BRUSH_CURSOR
#define MILTON_HARDWARE_BRUSH_CURSOR 0
#endif

// Uses GL 2.1 when 0
#define USE_GL_3_2 1


    // Use 3.2 on macos. OpenGL 3.2 is supported by all mac computers since macOS 10.8.5
    // https://developer.apple.com/opengl/OpenGL-Capabilities-Tables.pdf
    #if defined(__MACH__)
        #undef USE_GL_3_2
        #define USE_GL_3_2 1
    #endif


// Spawn threads to save the canvas.
#define MILTON_SAVE_ASYNC 1

// NOTE: Multisampling is no longer supported in Milton. This define is left
// in because there is some helper code which I would prefer not to delete.
#define MULTISAMPLING_ENABLED 0
    #define MSAA_NUM_SAMPLES 4



// When not in debug mode, disable all debug flags.

#if !MILTON_DEBUG

    #undef MILTON_ZOOM_DEBUG
    #define MILTON_ZOOM_DEBUG 0

    #undef WIN32_DEBUGGER_OUTPUT
    #define WIN32_DEBUGGER_OUTPUT 0

    #undef USE_GL_3_2
    #define USE_GL_3_2 0

    #undef REDRAW_EVERY_FRAME
    #define REDRAW_EVERY_FRAME 0

    #undef STROKE_DEBUG_VIZ
    #define STROKE_DEBUG_VIZ 0

    #undef MILTON_ENABLE_PROFILING
    #define MILTON_ENABLE_PROFILING 0

#endif
