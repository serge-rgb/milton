// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include <stb_image_write.h>

#include <imgui.h>


#include "tiny_jpeg.h"

#include "system_includes.h"

#include "milton_configuration.h"

#include "common.h"
#include "memory.h"

#include "DArray.h"
#include "Vector.h"

#include "vector.h"
#include "vector.cc"

#include "utils.h"


#include "platform.h"
#if defined(_WIN32)
#include "platform_windows.cc"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.cc"
#endif

#undef main // Defined by SDL

//#include "darray_test.cc"

#include "utils.cc"
#include "localization.cc"
#include "gl_helpers.h"
#include "gl_helpers.cc"
#include "gl_func_loader.cc"
#include "render_common.h"
#include "canvas.h"
#include "canvas.cc"
#include "profiler.h"
#include "profiler.cc"
#include "milton.h"
#include "color.h"
#include "color.cc"
#include "software_renderer.h"
#include "gui.h"
#include "hardware_renderer.cc"
#include "persist.cc"
#include "gui.cc"
#include "software_renderer.cc"
#include "memory.h"
#include "memory.cc"
#if MILTON_DEBUG
#include "tests.cc"
#endif
#include "milton.cc"


#include "sdl_milton.cc"


#undef GetWindowFont
#pragma warning(push,0)
#include "../third_party/imgui/imgui.cpp"
#include "../third_party/imgui/imgui_draw.cpp"
#include "../third_party/imgui/imgui_impl_sdl_gl3.cpp"
#undef snprintf


extern "C"
{

#define EASYTAB_IMPLEMENTATION
#include "easytab.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

}

#pragma warning(pop)
