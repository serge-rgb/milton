// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "system_includes.h"
#include "milton_configuration.h"
#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

#include "platform.h"

#include "gl_helpers.h"

#include "utils.h"
#include "canvas.h"
#include "canvas.cc"
#include "render_common.h"
#include "memory.h"
#include "milton.h"
#include "gui.h"

#include "localization.h"

#include "persist.h"

#include "color.h"

#include "software_renderer.h"

#if defined(__cplusplus)
}
#endif

#include "hardware_renderer.cc"
#include "guipp.cc"
#include "sdl_milton.cpp"
