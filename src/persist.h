// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "common.h"

#include "milton.h"

#if defined(__cplusplus)
extern "C" {
#endif


void milton_load(MiltonState* milton_state);

void milton_save(MiltonState* milton_state);

void milton_save_buffer_to_file(char* fname, u8* buffer, i32 w, i32 h);

// Open Milton with the last used canvas.
void milton_set_last_canvas_fname(char* last_fname);
void milton_unset_last_canvas_fname();

char* milton_get_last_canvas_fname();


#if defined(__cplusplus)
}
#endif
