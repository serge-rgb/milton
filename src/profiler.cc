// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


void
profiler_reset()
{
#if defined(PROFILER_IMPLEMENTATION)
    for ( i32 i = 0; i < PROF_COUNT; ++i ) {
        g_profiler_count[i] = 0;
    }
#endif
}


void
profiler_init()
{
#if defined(PROFILER_IMPLEMENTATION)
    for( i64 i=0; i<PROF_COUNT; ++i ) {
        g_profiler_ticks[i] = 0;
        g_profiler_last[i]  = 0;
        g_profiler_count[i] = 0;
    }
#endif
}


