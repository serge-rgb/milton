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





// ========
//
// Keeping this in its own file just to check for bugs in the separation of
// declarations and implementation in EasyTab.h


#if defined(_WIN32)
#include <Windows.H>
#endif

#define EASYTAB_IMPLEMENTATION
#include "easytab.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "../third_party/imgui/imgui.cpp"
#include "../third_party/imgui/imgui_draw.cpp"

#define TJE_IMPLEMENTATION
#include "../src/tiny_jpeg.h"
