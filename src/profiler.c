// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "profiler.h"
#include "platform.h"

#if defined(PROFILER_IMPLEMENTATION)
u64 g_profiler_ticks[MILTON_PROFILER_COUNT];  // Total cpu clocks
u64 g_profiler_count[MILTON_PROFILER_COUNT];  // How many calls
#endif


void profiler_output()
{
#if defined(PROFILER_IMPLEMENTATION)
    milton_log("===== Profiler output ==========\n");
    for (i32 i = 0; i < MILTON_PROFILER_COUNT; ++i) {
        if (g_profiler_count[i]) {
            milton_log("%15s: %15s %15lu, %15s %15lu\n",
                       g_profiler_names[i],
                       "ncalls:", g_profiler_count[i],
                       "clocks_per_call:", g_profiler_ticks[i] / g_profiler_count[i]);
                       //"total clocks:", g_profiler_ticks[i]);
        }
    }
    milton_log("================================\n");
#endif
}


