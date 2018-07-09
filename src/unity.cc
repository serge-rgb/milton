// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "StrokeList.cc"
#include "canvas.cc"
#include "color.cc"
#include "gl_helpers.cc"
#include "gui.cc"
#include "localization.cc"
#include "memory.cc"
#include "milton.cc"
#include "persist.cc"
#include "profiler.cc"
#include "renderer.cc"
#include "sdl_milton.cc"
#include "utils.cc"
#include "vector.cc"

#if defined(_WIN32)
   #include "platform_windows.cc"
#elif defined(__linux__)
   #include "platform_unix.cc"
   #include "platform_linux.cc"
#endif

#include "third_party_libs.cc"
