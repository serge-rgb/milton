// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once


#define MILTON_MAJOR_VERSION 1
#define MILTON_MINOR_VERSION 5
#define MILTON_MICRO_VERSION 2

#define MILTON_DEBUG 1

#define MILTON_ZOOM_DEBUG 0
    // If MILTON_DEBUG is 0, MILTON_ZOOM_DEBUG will be 0 too!
#if !MILTON_DEBUG
    #undef MILTON_ZOOM_DEBUG;
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

#define MILTON_ENABLE_PROFILING 0

#define REDRAW_EVERY_FRAME 0

#define MILTON_HARDWARE_BRUSH_CURSOR 1
#if defined(__linux__)  // No support for system cursor on linux.
#undef MILTON_HARDWARE_BRUSH_CURSOR
#define MILTON_HARDWARE_BRUSH_CURSOR 0
#endif

// Uses GL 2.1 when 0
#define USE_GL_3_2 1
    #if !MILTON_DEBUG  // Don't use 3.2 in release.
        #undef USE_GL_3_2
        #define USE_GL_3_2 0
    #endif

#define DEBUG_MEMORY_USAGE 0

// -- Software renderer config..
    #define MAX_NUM_WORKERS 64
    // Force things to be a bit slower
    #define RESTRICT_NUM_WORKERS_TO_2 0

// Large files get impractical to save in a blocking function.
// Disabled for now. This was implemented when a bug was causing files to be huge.
// Hopefully milton will never need this. Leaving it just in case.
#define MILTON_SAVE_ASYNC 1

// Include the software renderer.
#define SOFTWARE_RENDERER_COMPILED 0
// NOTE: software renderer has accumulated some bit rot..
// TODO: Remove software renderer from codebase.


// -- Esoteric and/or Stupid stuff.


// Rasterizer profiling -
// Self-profiling counters. Currently only active on Windows. Should be used
// for micro-tweaks. Use a sampling profiler for higher-level performance info.
// Dumps information out to Windows console.
#define MILTON_ENABLE_RASTER_PROFILING 0

// Every render_canvas call will re-draw using all renderers (for sampling profilers)
#define MILTON_USE_ALL_RENDERERS 0

#if MILTON_ENABLE_RASTER_PROFILING
    #if MILTON_MULTITHREADED
        #undef MILTON_MULTITHREADED
        #define MILTON_MULTITHREADED 0
    #endif  // MILTON_MULTITHREADED != 0
#endif  // MILTON_ENABLE_PROFILING
