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


#if defined(__cplusplus)
}
#endif
