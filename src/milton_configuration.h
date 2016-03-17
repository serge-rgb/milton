// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

// Self-profiling counters. Currently only active on Windows. Should be used
// for micro-tweaks. Use a sampling profiler for higher-level performance info
#define MILTON_ENABLE_PROFILING 0

// When MILTON_DEBUG is 1,
//  -
#define MILTON_DEBUG 1

// Yup
#define MILTON_MULTITHREADED 1

// 0 - Use strict SRGB definition
// 1 - Use a power curve of 2.
#define FAST_GAMMA 0


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


