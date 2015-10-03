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

#include "profiler.h"


void profiler_output()
{
#if defined(PROFILER_ENABLED)
    milton_log("===== Profiler output ==========\n");
    for (i32 i = 0; i < MILTON_PROFILER_COUNT; ++i) {
        if (g_profiler_count[i]) {
            milton_log("%20s: %20s %20lu, %20s %20lu\n",
                       g_profiler_names[i],
                       "ncalls:", g_profiler_count[i],
                       "clocks_per_call:", g_profiler_ticks[i] / g_profiler_count[i]);
        }
    }
    milton_log("================================\n");
#endif
}


