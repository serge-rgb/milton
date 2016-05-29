// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#if defined(_WIN32)
#include "platform_windows.c"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.c"
#endif

#include "canvas.c"
#include "color.c"
#include "gui.c"
#include "utils.c"
#include "history_debugger.c"
#include "milton.c"
#include "sb_grow.c"
#include "gl_helpers.c"
#include "memory.c"
#include "persist.c"
#include "software_renderer.c"
#include "vector.c"
#include "localization.c"
#include "profiler.c"

#if MILTON_DEBUG
#include "tests.c"
#endif
