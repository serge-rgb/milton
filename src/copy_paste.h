// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

typedef u32 SelectionId;

struct Selection
{
    u16 num_points;
    v2f* points;
};

struct PastedStrokes
{
    i32 id;

    SelectionId selection_id;

    Rect bounds;
    v2l offset;

    u64 num_sources;
    i32* source_stroke_ids;
};

enum PastaFSM
{
    PastaFSM_EMPTY,
    PastaFSM_SELECTING,
    PastaFSM_READY,
};

struct CopyPaste
{
    Selection* selection;
    PastaFSM fsm;
};

void pasta_init(Arena*, CopyPaste* pasta);
void pasta_input(CopyPaste* pasta, MiltonInput* input);
void pasta_clear(CopyPaste* pasta);