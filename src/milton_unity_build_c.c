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


#if defined(_WIN32)
#include "platform_windows.c"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.c"
#endif

#include "canvas.c"
#include "color.c"
#include "gui.c"
#include "utils.c"
#include "milton.c"
#include "sb_grow.c"
#include "gl_helpers.c"
#include "memory.c"
#include "persist.c"
#include "rasterizer.c"
#include "tests.c"
#include "vector.c"
#include "profiler.c"
