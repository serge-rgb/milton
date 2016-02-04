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


