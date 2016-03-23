// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once


// Not the most fancy thing, but it's useful for quick and dirty sessions of optimization.

#include "milton_configuration.h"
#include "common.h"

typedef enum {
    MILTON_PROFILER_render_canvas,
    MILTON_PROFILER_sse2,
    MILTON_PROFILER_preamble,
    MILTON_PROFILER_load,
    MILTON_PROFILER_work,
    MILTON_PROFILER_gather,
    MILTON_PROFILER_sampling,
    MILTON_PROFILER_total_work_loop,
    MILTON_PROFILER_sample,
    MILTON_PROFILER_COUNT,
} ProfiledBlockID;

void profiler_output();
static char* g_profiler_names[MILTON_PROFILER_COUNT] = {
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

#if defined(_WIN64) && MILTON_ENABLE_PROFILING

#define PROFILER_IMPLEMENTATION

extern u64 g_profiler_ticks[MILTON_PROFILER_COUNT];  // Total cpu clocks
extern u64 g_profiler_count[MILTON_PROFILER_COUNT];  // How many calls


static u32 TSC_AUX;
#define PROFILE_BEGIN(name) u64 profile_##name##_start = __rdtscp(&TSC_AUX);
#define PROFILE_PUSH_(name, start)\
    g_profiler_count[MILTON_PROFILER_##name] += 1;\
    g_profiler_ticks[MILTON_PROFILER_##name] += __rdtscp(&TSC_AUX) - start;

#define PROFILE_PUSH(name) PROFILE_PUSH_(name, profile_##name##_start)


#else

#define PROFILE_BEGIN(name)
#define PROFILE_PUSH(name)

#endif

