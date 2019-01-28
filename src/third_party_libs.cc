// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#if defined(_WIN32)
#pragma warning(push,0)
#endif

    #include "../third_party/imgui/imgui.cpp"
    #include "../third_party/imgui/imgui_widgets.cpp"
    #include "../third_party/imgui/imgui_draw.cpp"
    #include "../third_party/imgui/imgui_impl_sdl.cpp"
    #include "../third_party/imgui/imgui_impl_opengl3.cpp"

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

#if defined(_WIN32)
#pragma warning(pop)
#endif
