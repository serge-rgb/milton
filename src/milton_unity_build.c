// ----
#include "SDL.h"
#include "SDL_syswm.h"


// Using stb_image to load our GUI resources.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// ----

#include "define_types.h"
#include "vector.generated.h"

#define MILTON_DESKTOP
#include "system_includes.h"

#include "platform.h"

#if defined(_WIN32)
#include "platform_windows.h"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.h"
#endif


#include "milton.h"

#include "StrokeCord.generated.c"
#include "canvas.c"
#include "color.c"
#include "gui.c"
#include "memory.c"
#include "milton.c"
#include "persist.c"
#include "profiler.c"
#include "rasterizer.c"
#include "sdl_milton.c"
#include "utils.c"


