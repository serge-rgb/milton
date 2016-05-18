// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#define MILTON_VERSION "1.2.2"

// When MILTON_DEBUG is 1,
//  - Asserts
//  - Fixed address root_arena
//  -  very large root_arena
//  - Runtime renderer implementation switching with F4
#define MILTON_DEBUG 1

#define MILTON_MULTITHREADED 1

#define MILTON_ENABLE_PROFILING 0

#define MAX_NUM_WORKERS 64
// Force things to be a bit slower
#define RESTRICT_NUM_WORKERS_TO_2 0


// 0 - Use strict SRGB definition
// 1 - Use a power curve of 2.
#define FAST_GAMMA 0

// Large files get impractical to save in a blocking function.
#define MILTON_SAVE_ASYNC 1


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


