
// ----
#include "SDL.h"
#include "SDL_syswm.h"


// Using stb_image to load our GUI resources.
#include "stb_image.h"
// ----


// EasyTab for drawing tablet support
#include "easytab.h"
// ----

// ImGUI
#

#include "define_types.h"
#include "vector.h"

#define MILTON_DESKTOP
#include "system_includes.h"

#include "platform.h"

#if defined(_WIN32)
#include "platform_windows.h"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.h"
#endif


#include "milton.h"

#include "canvas.cc"
#include "color.cc"
#include "gui.cc"
#include "memory.cc"
#include "milton.cc"
#include "persist.cc"
#include "profiler.cc"
#include "rasterizer.cc"
#include "sdl_milton.cc"
#include "tests.cc"
#include "utils.cc"
#include "../third_party/imgui/imgui_impl_sdl_gl3.cpp"

