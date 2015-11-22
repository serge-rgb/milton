// Keeping this in its own file just to check for bugs in the separation of
// declarations and implementation in EasyTab.h


#if defined(_WIN32)
#include <Windows.H>
#endif

#define EASYTAB_IMPLEMENTATION
#include "easytab.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "../third_party/imgui/imgui.cpp"
#include "../third_party/imgui/imgui_draw.cpp"
