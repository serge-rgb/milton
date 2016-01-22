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


#include "define_types.h"
#include "Vector.h"

#define MILTON_DESKTOP
#include "system_includes.h"

#include "platform.h"

// Using stb_image to load our GUI resources.
#include <stb_image.h>
// ----

// EasyTab for drawing tablet support
#include "easytab.h"
// ----


#if defined(_WIN32)
#include "platform_windows.h"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.h"
#endif


#include "milton.h"

#include "canvas.cc"
#include "color.cc"
#include "milton.cc"
#include "gui.cc"
#include "memory.cc"
#include "persist.cc"
#include "profiler.cc"
#include "rasterizer.cc"
#include "sdl_milton.cc"
#include "tests.cc"
#include "utils.cc"
#include "../third_party/imgui/imgui_impl_sdl_gl3.cpp"

