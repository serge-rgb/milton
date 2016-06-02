// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include <stb_image_write.h>

#include <imgui.h>

#include "tiny_jpeg.h"

#include "system_includes.h"

#include "milton_configuration.h"

#include "common.h"
#include "utils.h"


#include "platform.h"
#if defined(_WIN32)
#include "platform_windows.cc"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.cc"
#endif

#include "DArray.h"


#include "vector.cc"
#include "utils.h"
#include "utils.cc"

//#include "darray_test.cc"

#include "localization.cc"
#include "gl_helpers.h"
#include "gl_helpers.cc"
#include "render_common.h"
#include "canvas.h"
#include "canvas.cc"
#include "milton.h"
#include "gui.h"
#include "persist.h"
#include "persist.cc"
#include "color.h"
#include "color.cc"
#include "software_renderer.cc"
#include "hardware_renderer.cc"
#include "guipp.cc"
#include "memory.h"
#include "memory.cc"
#include "software_renderer.h"
#include "gui.cc"
#include "profiler.h"
#include "milton.cc"


#include "sdl_milton.cc"

#include "profiler.cc"

#if MILTON_DEBUG
#include "tests.cc"
#endif
