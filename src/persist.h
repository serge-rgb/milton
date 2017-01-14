#pragma once

#include "platform.h"

struct MiltonState;

PATH_CHAR* milton_get_last_canvas_fname();

void milton_load(MiltonState* milton_state);
void milton_save(MiltonState* milton_state);
void milton_save_buffer_to_file(PATH_CHAR* fname, u8* buffer, i32 w, i32 h);

void milton_prefs_load(PlatformPrefs* prefs);
void milton_prefs_save(PlatformPrefs* prefs);


