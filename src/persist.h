// persist.h
// (c) Copyright 2015 by Sergio Gonzalez


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

// TODO: handle failures gracefully.
static void milton_load(MiltonState* milton_state)
{
    FILE* fd = fopen("MiltonPersist.mlt", "rb");
    if (!fd)
    {
        milton_log("Warning. Did not find persist file.\n");
        return;
    }

    u32 milton_magic = (u32)-1;
    fread(&milton_magic, sizeof(u32), 1, fd);
    milton_magic = word_swap_memory_order(milton_magic);

    if (milton_magic != MILTON_MAGIC_NUMBER)
    {
        assert (!"Magic number not found");
        return;
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
        fread(stroke->points, sizeof(v2i), stroke->num_points, fd);
    }


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
