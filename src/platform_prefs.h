// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "common.h"

// Stuff that persists in [MILTONDIR]/prefs
typedef struct PlatformPrefs
{
    // Store the window size at the time of quitting.
    i32 width;
    i32 height;
} PlatformPrefs;
