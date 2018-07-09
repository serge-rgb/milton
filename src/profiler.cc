// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#if defined(_WIN32) && MILTON_ENABLE_PROFILING
u64 g_profiler_ticks[PROF_COUNT];
u64 g_profiler_last[PROF_COUNT];
u64 g_profiler_count[PROF_COUNT];
#endif


#if MILTON_ENABLE_PROFILING

void
profiler_reset()
{
#if defined(_WIN32) && MILTON_ENABLE_PROFILING
    for ( i32 i = 0; i < PROF_COUNT; ++i ) {
        g_profiler_count[i] = 0;
    }
#endif
}


void
profiler_init()
{
#if defined(_WIN32) && MILTON_ENABLE_PROFILING
    for( i64 i=0; i<PROF_COUNT; ++i ) {
        g_profiler_ticks[i] = 0;
        g_profiler_last[i]  = 0;
        g_profiler_count[i] = 0;
    }
#endif
}


#endif
