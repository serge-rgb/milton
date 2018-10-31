// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "platform.h"

struct Milton;
struct MiltonSettings;

PATH_CHAR* milton_get_last_canvas_fname();

void milton_load(Milton* milton);
void milton_save(Milton* milton);
void milton_save_v6(Milton* milton);  // TODO: Remove this once the new save format is done
void milton_save_buffer_to_file(PATH_CHAR* fname, u8* buffer, i32 w, i32 h);

b32  milton_appstate_load(PlatformPrefs* prefs);
void milton_appstate_save(PlatformPrefs* prefs);

void milton_settings_load(MiltonSettings* settings);
void milton_settings_save(MiltonSettings* settings);

