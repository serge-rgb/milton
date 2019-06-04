// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

typedef u32 SelectionId;

struct Selection
{
    u16 num_points;
    v2l* points;
};

struct PastedStrokes
{
    i32 id;

    SelectionId selection_id;

    u64 num_sources;
    i32* source_stroke_ids;
};

struct CopyPaste
{
    Selection* selection;
};

void pasta_init(Arena*, CopyPaste* pasta);
