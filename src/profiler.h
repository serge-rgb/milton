//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once


// Not the most fancy thing, but it's useful for quick and dirty sessions of optimization.

#include "define_types.h"


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

#if defined(_WIN64) && 1

#define PROFILER_ENABLED

static u64 g_profiler_ticks[MILTON_PROFILER_COUNT];  // Total cpu clocks
static u64 g_profiler_count[MILTON_PROFILER_COUNT];  // How many calls


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

