// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "platform.h"

struct Milton;
struct MiltonSettings;

struct MiltonPersist
{
    // Persistence
    PATH_CHAR*  mlt_file_path;
    u32         mlt_binary_version;
    WallTime    last_save_time;
    i64         last_save_stroke_count; // This is a workaround to MoveFileEx failing occasionally, particularly
                                        // when the mlt file gets large.
                                        // Check that all the strokes are saved at quit time in case
                                        // the last MoveFileEx failed.
    float target_MB_per_sec;

    sz bytes_to_last_block;
};

PATH_CHAR* milton_get_last_canvas_fname();

void milton_load(Milton* milton);
u64 milton_save(Milton* milton);

void milton_save_buffer_to_file(PATH_CHAR* fname, u8* buffer, i32 w, i32 h);

b32  platform_settings_load(PlatformSettings* prefs);
void platform_settings_save(PlatformSettings* prefs);

b32 milton_settings_load(MiltonSettings* settings);
void milton_settings_save(MiltonSettings* settings);


