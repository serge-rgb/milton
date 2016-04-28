// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "common.h"

struct SDL_Cursor;
struct PlatformState
{
    i32 width;
    i32 height;

    b32 is_ctrl_down;
    b32 is_shift_down;
    b32 is_space_down;
    b32 is_pointer_down;

    int panning_fsm;

    b32 is_panning;
    b32 panning_locked; // locked when panning from GUI

    b32 was_exporting;
    v2i pan_start;
    v2i pan_point;

    b32 should_quit;
    u32 window_id;

    i32 num_pressure_results;
    i32 num_point_results;
    b32 stopped_panning;

    // SDL Cursors
    SDL_Cursor* cursor_default;
    SDL_Cursor* cursor_hand;
    SDL_Cursor* cursor_crosshair;
    SDL_Cursor* cursor_sizeall;
};

#if defined(__cplusplus)
}
#endif
