#pragma once

// Self-profiling counters. Currently only active on Windows. Should be used
// for micro-tweaks. Use a sampling profiler for higher-level performance info
#define MILTON_ENABLE_PROFILING 0

// Yup
#define MILTON_MULTITHREADED 1


// By default, when activating the internal profiler with
// MILTON_ENABLE_PROFILING, we will turn miltithreading off. If MT is ever
// needed for some reason, all we have to do is us atomic ops in the right
// places in profiler.(c|h)
#if MILTON_ENABLE_PROFILING
    #if MILTON_MULTITHREADED
        #undef MILTON_MULTITHREADED
        #define MILTON_MULTITHREADED 0
    #endif  // MILTON_MULTITHREADED != 0
#endif  // MILTON_ENABLE_PROFILING


