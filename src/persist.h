// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "platform.h"

struct Milton;
struct MiltonSettings;

#define NUM_FIXED_BLOCKS 3  // Picker, buttons, brushes.
enum BlockType
{
    // Fixed-size and fixed-position
    Block_COLOR_PICKER,
    Block_BUTTONS,
    Block_BRUSHES,

    // Dynamic size and movable
    Block_PAINTING_DESCRIPTION,
    Block_LAYER_CONTENT,
};

#pragma pack(push, 1)
struct BlockLayer
{
    i32 id;
};

struct SaveBlockHeader
{
    u16 type; /*BlockType*/

    // MLT version 6
    union
    {
        // TODO: if layer content is the only one who uses this, maybe it would be better to save block_layer outside of the header.
        BlockLayer block_layer;  // Block_LAYER_CONTENT
    };

};
#pragma pack(pop)


struct SaveBlock
{
    SaveBlockHeader header;

    u64 bytes_begin;
    u64 bytes_end;

    u8 dirty;

    union
    {
        // Block_LAYER_CONTENT
        u64 num_saved_strokes;
    };
};


struct MiltonPersist
{
    // Persistence
    PATH_CHAR*  mlt_file_path;
    u32         mlt_binary_version;
    WallTime    last_save_time;
    i64         last_save_stroke_count;  // This is a workaround to MoveFileEx failing occasionally, particularaly when
                                        // when the mlt file gets large.
                                        // Check that all the strokes are saved at quit time in case that
                                        // the last MoveFileEx failed.
    DArray<SaveBlock> blocks;

    //u16 save_id;

    float target_MB_per_sec;

    sz bytes_to_last_block;
};

PATH_CHAR* milton_get_last_canvas_fname();

void milton_load(Milton* milton);
u64 milton_save(Milton* milton);

void milton_save_buffer_to_file(PATH_CHAR* fname, u8* buffer, i32 w, i32 h);

b32  milton_appstate_load(PlatformPrefs* prefs);
void milton_appstate_save(PlatformPrefs* prefs);

b32 milton_settings_load(MiltonSettings* settings);
void milton_settings_save(MiltonSettings* settings);


