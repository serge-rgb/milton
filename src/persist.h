// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "platform.h"

struct Milton;
struct MiltonSettings;

enum BlockType
{
    Block_INVALID,

    Block_COLOR_PICKER,
    Block_BUTTONS,
    Block_BRUSHES,
    Block_PAINTING_DESCRIPTION,
    Block_LAYER_CONTENT,
};

#pragma pack(push, 1)
struct SaveBlockHeader
{
    u16 type; /*BlockType*/

    union
    {
        struct BlockLayer
        {
            i32 id;
        } block_layer;
    };

};
#pragma pack(pop)

enum
{
    SaveBlock_DIRTY,
};

struct SaveBlock
{
    SaveBlockHeader header;

    u16 save_id;
};


struct MiltonPersist
{
    // Persistence
    bool DEV_use_new_format_read;
    bool DEV_use_new_format_write;
    PATH_CHAR*  mlt_file_path;
    u32         mlt_binary_version;
    WallTime    last_save_time;
    i64         last_save_stroke_count;  // This is a workaround to MoveFileEx failing occasionally, particularaly when
                                        // when the mlt file gets large.
                                        // Check that all the strokes are saved at quit time in case that
                                        // the last MoveFileEx failed.
    DArray<SaveBlock> blocks;

    u16 save_id;

    sz bytes_to_last_block;
};

PATH_CHAR* milton_get_last_canvas_fname();

void milton_load(Milton* milton);
void milton_save(Milton* milton);

void milton_load_v6_file(Milton* milton, PATH_CHAR* path);

void milton_save_v6(Milton* milton);  // TODO: Remove this once the new save format is done
void milton_save_v6_file(Milton* milton, PATH_CHAR* path);

void milton_save_buffer_to_file(PATH_CHAR* fname, u8* buffer, i32 w, i32 h);

b32  milton_appstate_load(PlatformPrefs* prefs);
void milton_appstate_save(PlatformPrefs* prefs);

void milton_settings_load(MiltonSettings* settings);
void milton_settings_save(MiltonSettings* settings);

void milton_persist_set_blocks_for_painting(Milton* milton);

void milton_mark_block_for_save(MiltonPersist* p, SaveBlockHeader header);

