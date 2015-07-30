//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#define MILTON_MAGIC_NUMBER 0X11DECAF3

inline u32 word_swap_memory_order(u32 word)
{
    u8 llo = (u8) (word & 0x000000ff);
    u8 lhi = (u8)((word & 0x0000ff00) >> 8);
    u8 hlo = (u8)((word & 0x00ff0000) >> 16);
    u8 hhi = (u8)((word & 0xff000000) >> 24);
    return
            (u32)(llo) << 24 |
            (u32)(lhi) << 16 |
            (u32)(hlo) << 8  |
            (u32)(hhi);
}

static void milton_load(MiltonState* milton_state)
{
    FILE* fd = fopen("MiltonPersist.mlt", "rb");
    if (!fd)
    {
        milton_log("Warning. Did not find persist file.\n");
        goto end;
    }

    u32 milton_magic = (u32)-1;
    fread(&milton_magic, sizeof(u32), 1, fd);

    fread(milton_state->view, sizeof(CanvasView), 1, fd);

    milton_magic = word_swap_memory_order(milton_magic);

    if (milton_magic != MILTON_MAGIC_NUMBER)
    {
        assert (!"Magic number not found");
        goto end;
    }

    i32 num_strokes = -1;
    fread(&num_strokes, sizeof(i32), 1, fd);

    assert (num_strokes >= 0);

    milton_state->num_strokes = num_strokes;

    for (i32 stroke_i = 0; stroke_i < num_strokes; ++stroke_i)
    {
        Stroke* stroke = &milton_state->strokes[stroke_i];
        fread(&stroke->brush, sizeof(Brush), 1, fd);
        fread(&stroke->num_points, sizeof(i32), 1, fd);
        if (stroke->num_points >= STROKE_MAX_POINTS ||
            stroke->num_points <= 0)
        {
            // Corrupt file. Avoid this read
            stroke_i--;     // Pretend this never happened
            continue;       // Do not allocate, just move on.
        }
        stroke->points = arena_alloc_array(milton_state->root_arena, stroke->num_points, v2i);
        fread(stroke->points, sizeof(v2i), stroke->num_points, fd);
    }

end:
    fclose(fd);
}

// TODO: handle failures gracefully.
static void milton_save(MiltonState* milton_state)
{
    i32 num_strokes = milton_state->num_strokes;
    Stroke* strokes = milton_state->strokes;
    Stroke* working_stroke = &milton_state->working_stroke;
    FILE* fd = fopen("MiltonPersist.mlt", "wb");

    if (!fd)
    {
        assert (!"Could not create file");
        return;
    }

    // Note: assuming little-endian.
    u32 milton_magic = word_swap_memory_order(MILTON_MAGIC_NUMBER);

    fwrite(&milton_magic, sizeof(u32), 1, fd);

    fwrite(milton_state->view, sizeof(CanvasView), 1, fd);

    fwrite(&num_strokes, sizeof(i32), 1, fd);

    for (i32 stroke_i = 0; stroke_i < num_strokes; ++stroke_i)
    {
        Stroke* stroke = &strokes[stroke_i];
        fwrite(&stroke->brush, sizeof(Brush), 1, fd);
        fwrite(&stroke->num_points, sizeof(i32), 1, fd);
        fwrite(stroke->points, sizeof(v2i), stroke->num_points, fd);
    }

    fclose(fd);
}

#ifdef __cplusplus
}
#endif
