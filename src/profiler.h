// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once



// Profiler
//
// The PROFILE_RASTER_BEGIN and PROFILE_RASTER_PUSH macros report the rasterizer performance
// to the Windows console output.
//
// The PROFILE_GRAPH_BEGIN and PROFILE_GRAPH_END macros write to the
// g_profiler_last array.
//
//
enum
{
    PROF_RASTER_render_canvas,
    PROF_RASTER_sse2,
    PROF_RASTER_preamble,
    PROF_RASTER_load,
    PROF_RASTER_work,
    PROF_RASTER_gather,
    PROF_RASTER_sampling,
    PROF_RASTER_total_work_loop,
    PROF_RASTER_sample,


    PROF_RASTER_COUNT, // Number of raster profiler counters for Win32 Console output


    PROF_GRAPH_polling=0,     // Time spent gathering input.
    PROF_GRAPH_update,      // Time spent processing input.
    PROF_GRAPH_raster,      // Time spent rasterizing the canvas.
    PROF_GRAPH_GL,          // Time spent sending data and draw commands to GL

    PROF_COUNT = PROF_RASTER_COUNT+4,
    //PROF_COUNT,

};


struct GraphData
{
    u64 start;


    u64 polling;
    u64 update;
    u64 clipping;
    u64 raster;
    u64 GL;
    u64 system;
};

extern GraphData g_graphframe;

static char* g_profiler_names[PROF_RASTER_COUNT] =
{
    "render_canvas",
    "sse2",
    "preamble",
    "load",
    "work",
    "gather",
    "sampling",
    "total_work_loop",
    "sample",
};

void profiler_init();
void profiler_output();
void profiler_reset();

#if defined(_WIN64) && MILTON_ENABLE_PROFILING
    extern u64 g_profiler_ticks[PROF_COUNT];     // Total cpu clocks
    extern u64 g_profiler_last[PROF_COUNT];
    extern u64 g_profiler_count[PROF_COUNT];     // How many calls


    static u32 TSC_AUX;
    static int CPUID_AUX1[4];
    static int CPUID_AUX2;

    /////////
    #define PROFILE_GRAPH_BEGIN(name) \
            milton->graph_frame.start = perf_counter();

    #define PROFILE_GRAPH_END(name)  \
            milton->graph_frame.##name = perf_counter() - milton->graph_frame.start

#elif defined(__linux__) && MILTON_ENABLE_PROFILING

    #define PROFILE_GRAPH_BEGIN(name) \
            milton->graph_frame.start = perf_counter();

    #define PROFILE_GRAPH_END(name)  \
            milton->graph_frame.name = perf_counter() - milton->graph_frame.start

#else

#define PROFILE_GRAPH_BEGIN(name)
#define PROFILE_GRAPH_END(name)

#endif

