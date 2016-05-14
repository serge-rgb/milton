// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once


// Not the most fancy thing, but it's useful for quick and dirty sessions of optimization.

#include "milton_configuration.h"
#include "common.h"

#if defined(__cplusplus)
extern"C"{
#endif

// Profiler
//
// The PROFILE_BEGIN and PROFILE_PUSH macros report the rasterizer performance
// to the Windows console output.
//
// The GRAPH_BEGIN and GRAPH_PUSH macros use the same "guts"
// TODO: docs
//
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

    PROF_GRAPH_test,
    PROF_COUNT,
};


static char* g_profiler_names[PROF_COUNT] =
{
    "render_canvas",
    "sse2",
    "preamble",
    "load",
    "work",
    "gather",
    "sampling",
    "total_work_loop",
    "sample"
};

void profiler_output();
void profiler_reset();

#if defined(_WIN64) && MILTON_ENABLE_PROFILING

#define PROFILER_IMPLEMENTATION

extern u64 g_profiler_ticks[PROF_COUNT];     // Total cpu clocks
extern u64 g_profiler_last[PROF_COUNT];
extern u64 g_profiler_count[PROF_COUNT];     // How many calls


static u32 TSC_AUX;
static int CPUID_AUX1[4];
static int CPUID_AUX2;
#define PROFILE_RASTER_BEGIN(name) __cpuid(CPUID_AUX1, CPUID_AUX2); u64 profile_##name##_start = __rdtsc();
#define PROFILE_RASTER_PUSH_(name, start)\
    g_profiler_count[PROF_RASTER_##name] += 1;\
    u64 profile_##name##_ncycles = __rdtscp(&TSC_AUX) - start; \
    g_profiler_ticks[PROF_RASTER_##name] += profile_##name##_ncycles; \
    g_profiler_last[PROF_RASTER_##name] = profile_##name##_ncycles;

#define PROFILE_RASTER_PUSH(name) PROFILE_RASTER_PUSH_(name, profile_##name##_start)

#else

#define PROFILE_RASTER_BEGIN(name)
#define PROFILE_RASTER_PUSH(name)

#endif

#if defined(__cplusplus)
}
#endif
