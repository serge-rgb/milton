// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "persist.h"

#include <stb_image_write.h>

#include "common.h"
#include "gui.h"
#include "memory.h"
#include "milton.h"
#include "platform.h"
#include "tiny_jpeg.h"


#define MILTON_MAGIC_NUMBER 0X11DECAF3

#if MILTON_DEBUG_SAVE
static i32 g_bytes_written = 0;

void debug_mark_bytes(u64 bytes)
{
    g_bytes_written += bytes;
}

void save_debug_log(char* message, ...)
{
    va_list args;
    va_start(args, message);
    milton_log_args(message, args);
    va_end(args);
}

#else

void debug_mark_bytes(u64 bytes) {}

#endif

#pragma pack(push, 1)
struct PersistStrokePoint
{
    v2l point;
    f32 pressure;
};
#pragma pack(pop)

static b32
fread_checked(void* dst, size_t sz, size_t count, FILE* fd)
{
    b32 ok = false;

    size_t read = fread(dst, sz, count, fd);
    if ( read == count && !ferror(fd)) {
        ok = true;
    }

    return ok;
}

static b32
fwrite_checked(void* data, size_t sz, size_t count, FILE* fd)
{
    b32 ok = false;
    size_t written = fwrite(data, sz, count, fd);
    if ( written == count ) {
        ok = true;
    }

    return ok;
}

void
milton_unset_last_canvas_fname()
{
    b32 del = platform_delete_file_at_config(TO_PATH_STR("saved_path"), DeleteErrorTolerance_OK_NOT_EXIST);
    if ( del == false ) {
        platform_dialog("The default canvas could not be set to open the next time you run Milton. Please contact the developers.", "Important");
    }
}

void
milton_load(Milton* milton)
{
    // Declare variables here to silence compiler warnings about using GOTO.
    i32 history_count = 0;
    i32 num_layers = 0;
    i32 saved_working_layer_id = 0;
    int err = 0;

    i32 layer_guid = 0;
    ColorButton* btn = NULL;
    MiltonGui* gui = NULL;
    auto saved_size = milton->view->screen_size;

    milton_log("Loading file %s\n", milton->persist->mlt_file_path);
    // Reset the canvas.
    milton_reset_canvas(milton);

    CanvasState* canvas = milton->canvas;
#define READ(address, size, num, fd) do { ok = fread_checked(address,size,num,fd); if (!ok){ goto END; } } while(0)

    // Unload gpu data if the strokes have been cooked.
    gpu_free_strokes(milton->render_data, milton->canvas);
    mlt_assert(milton->persist->mlt_file_path);
    FILE* fd = platform_fopen(milton->persist->mlt_file_path, TO_PATH_STR("rb"));
    b32 ok = true;  // fread check
    b32 handled = false;  // when ok==false but we don't need to prompt a scary message.

    if ( fd ) {
        u32 milton_binary_version = (u32)-1;
        u32 milton_magic = (u32)-1;
        READ(&milton_magic, sizeof(u32), 1, fd);
        READ(&milton_binary_version, sizeof(u32), 1, fd);

        if (ok) {
            if ( milton_binary_version < 4 ) {
                if ( platform_dialog_yesno ("This file will be updated to the new version of Milton. Older versions won't be able to open it. Is this OK?", "File format change") ) {
                    milton->persist->mlt_binary_version = MILTON_MINOR_VERSION;
                    milton_log("Updating this file to latest mlt version.\n");
                } else {
                    ok = false;
                    handled = true;
                    goto END;
                }
            } else {
                milton->persist->mlt_binary_version = milton_binary_version;
            }
        }

        if ( milton_binary_version > MILTON_MINOR_VERSION ) {
            platform_dialog("This file was created with a newer version of Milton.", "Could not open.");

            // Stop loading, but exit without prompting.
            ok = false;
            handled = true;
            goto END;
        }

        if ( milton_binary_version >= 4 ) {
            READ(milton->view, sizeof(CanvasView), 1, fd);
        } else {
            CanvasViewPreV4 legacy_view = {};
            READ(&legacy_view, sizeof(CanvasViewPreV4), 1, fd);
            milton->view->screen_size = legacy_view.screen_size;
            milton->view->scale = legacy_view.scale;
            milton->view->zoom_center = legacy_view.zoom_center;
            milton->view->pan_center = VEC2L(legacy_view.pan_center * -1);
            milton->view->background_color = legacy_view.background_color;
            milton->view->working_layer_id = legacy_view.working_layer_id;
            milton->view->num_layers = legacy_view.num_layers;
        }

        // The screen size might hurt us.
        milton->view->screen_size = saved_size;

        // The process of loading changes state. working_layer_id changes when creating layers.
        saved_working_layer_id = milton->view->working_layer_id;

        if ( milton_magic != MILTON_MAGIC_NUMBER ) {
            platform_dialog("MLT file could not be loaded. Magic number mismatch.", "Problem");
            milton_unset_last_canvas_fname();
            ok = false;
            goto END;
        }

        num_layers = 0;
        READ(&num_layers, sizeof(i32), 1, fd);
        READ(&layer_guid, sizeof(i32), 1, fd);

        for ( int layer_i = 0; ok && layer_i < num_layers; ++layer_i ) {
            i32 len = 0;
            READ(&len, sizeof(i32), 1, fd);

            if ( len > MAX_LAYER_NAME_LEN ) {
                milton_log("Corrupt file. Layer name is too long.\n");
                ok = false;
                goto END;
            }

            if (ok) { milton_new_layer(milton); }

            Layer* layer = milton->canvas->working_layer;

            READ(layer->name, sizeof(char), (size_t)len, fd);

            READ(&layer->id, sizeof(i32), 1, fd);
            READ(&layer->flags, sizeof(layer->flags), 1, fd);

            if ( ok ) {
                i32 num_strokes = 0;
                READ(&num_strokes, sizeof(i32), 1, fd);

                for ( i32 stroke_i = 0; ok && stroke_i < num_strokes; ++stroke_i ) {
                    Stroke stroke = Stroke{};

                    stroke.id = milton->canvas->stroke_id_count++;

                    READ(&stroke.brush, sizeof(Brush), 1, fd);
                    READ(&stroke.num_points, sizeof(i32), 1, fd);

                    if ( stroke.num_points > STROKE_MAX_POINTS || stroke.num_points <= 0 ) {
                        milton_log("ERROR: File has a stroke with %d points\n",
                                   stroke.num_points);
                        // Older versions have a possible off-by-one bug here.
                        if (stroke.num_points <= STROKE_MAX_POINTS)  {
                            stroke.points = arena_alloc_array(&canvas->arena, stroke.num_points, v2l);
                            READ(stroke.points, sizeof(v2l), (size_t)stroke.num_points, fd);
                            stroke.pressures = arena_alloc_array(&canvas->arena, stroke.num_points, f32);
                            READ(stroke.pressures, sizeof(f32), (size_t)stroke.num_points, fd);
                            READ(&stroke.layer_id, sizeof(i32), 1, fd);
#if STROKE_DEBUG_VIZ
                            stroke.debug_flags = arena_alloc_array(&canvas->arena, stroke.num_points, int);
#endif

                            stroke.bounding_rect = bounding_box_for_stroke(&stroke);

                            layer::layer_push_stroke(layer, stroke);
                        } else {
                            ok = false;
                            goto END;
                        }
                    } else {
                        if ( milton_binary_version >= 4 ) {
                            stroke.points = arena_alloc_array(&canvas->arena, stroke.num_points, v2l);
                            READ(stroke.points, sizeof(v2l), (size_t)stroke.num_points, fd);
                        } else {
                            stroke.points = arena_alloc_array(&canvas->arena, stroke.num_points, v2l);
                            v2i* points_32bit = (v2i*)mlt_calloc((size_t)stroke.num_points, sizeof(v2i), "Persist");

                            READ(points_32bit, sizeof(v2i), (size_t)stroke.num_points, fd);
                            for (int i = 0; i < stroke.num_points; ++i) {
                                stroke.points[i] = VEC2L(points_32bit[i]);
                            }
                        }
#if STROKE_DEBUG_VIZ
                        stroke.debug_flags = arena_alloc_array(&canvas->arena, stroke.num_points, int);
#endif
                        stroke.pressures = arena_alloc_array(&canvas->arena, stroke.num_points, f32);
                        READ(stroke.pressures, sizeof(f32), (size_t)stroke.num_points, fd);
                        READ(&stroke.layer_id, sizeof(i32), 1, fd);
                        stroke.bounding_rect = bounding_box_for_stroke(&stroke);
                        layer::layer_push_stroke(layer, stroke);
                    }
                }
            }

            if ( milton_binary_version >= 4 ) {
                i64 num_effects = 0;
                READ(&num_effects, sizeof(num_effects), 1, fd);
                if ( num_effects > 0 ) {
                    LayerEffect** e = &layer->effects;
                    for ( i64 i = 0; i < num_effects; ++i ) {
                        mlt_assert(*e == NULL);
                        *e = arena_alloc_elem(&canvas->arena, LayerEffect);
                        READ(&(*e)->type, sizeof((*e)->type), 1, fd);
                        READ(&(*e)->enabled, sizeof((*e)->enabled), 1, fd);
                        switch ((*e)->type) {
                            case LayerEffectType_BLUR: {
                                READ(&(*e)->blur.original_scale, sizeof((*e)->blur.original_scale), 1, fd);
                                READ(&(*e)->blur.kernel_size, sizeof((*e)->blur.kernel_size), 1, fd);
                            } break;
                        }
                        e = &(*e)->next;
                    }
                }
            }
        }
        milton->view->working_layer_id = saved_working_layer_id;

        if ( milton_binary_version >= 5 ) {
           v3f rgb;
           READ(&rgb, sizeof(v3f), 1, fd);
           gui_picker_from_rgb(&milton->gui->picker, rgb);
        } else {
           READ(&milton->gui->picker.data, sizeof(PickerData), 1, fd);
        }


        // Buttons
        {
           i32 button_count = 0;
           gui = milton->gui;
           btn = gui->picker.color_buttons;

           READ(&button_count, sizeof(i32), 1, fd);
           for ( i32 i = 0;
                 btn!=NULL && i < button_count;
                 ++i, btn=btn->next ) {
              READ(&btn->rgba, sizeof(v4f), 1, fd);
           }
        }

        // Brush
        if ( milton_binary_version >= 2 && milton_binary_version <= 5  ) {
            // PEN, ERASER
            READ(&milton->brushes, sizeof(Brush), 2, fd);
            // Sizes
            READ(&milton->brush_sizes, sizeof(i32), 2, fd);
        }
        else if ( milton_binary_version > 5 ) {
            u16 num_brushes = 0;
            READ(&num_brushes, sizeof(u16), 1, fd);
            if ( num_brushes > BrushEnum_COUNT ) {
                milton_log("Error loading file: too many brushes: %d\n", num_brushes);
            }
            READ(&milton->brushes, sizeof(Brush), num_brushes, fd);
            READ(&milton->brush_sizes, sizeof(i32), num_brushes, fd);
        }

        history_count = 0;
        READ(&history_count, sizeof(history_count), 1, fd);
        reset(&milton->canvas->history);
        reserve(&milton->canvas->history, history_count);
        READ(milton->canvas->history.data, sizeof(*milton->canvas->history.data), (size_t)history_count, fd);
        milton->canvas->history.count = history_count;

        // MLT 3
        // Layer alpha
        if ( milton_binary_version >= 3 ) {
            Layer* l = milton->canvas->root_layer;
            for ( i64 i = 0; ok && i < num_layers; ++i ) {
                mlt_assert(l != NULL);
                READ(&l->alpha, sizeof(l->alpha), 1, fd);
                l = l->next;
            }
        } else {
            for ( Layer* l = milton->canvas->root_layer; l != NULL; l = l->next ) {
                l->alpha = 1.0f;
            }
        }

        err = fclose(fd);
        if ( err != 0 ) {
            ok = false;
        }

END:
        // Finished loading
        if ( !ok ) {
            if ( !handled ) {
                platform_dialog("Tried to load a corrupt Milton file or there was an error reading from disk.", "Error");
            }
            milton_reset_canvas_and_set_default(milton);
        } else {
            i32 id = milton->view->working_layer_id;
            {  // Use working_layer_id to make working_layer point to the correct thing
                Layer* layer = milton->canvas->root_layer;
                while ( layer ) {
                    if ( layer->id == id ) {
                        milton->canvas->working_layer = layer;
                        break;
                    }
                    layer = layer->next;
                }
            }
            milton->canvas->layer_guid = layer_guid;

            // Update GPU
            milton->flags |= MiltonStateFlags_JUST_SAVED;
        }
    } else {
        milton_log("milton_load: Could not open file!\n");
        milton_reset_canvas_and_set_default(milton);
    }
#undef READ
}

void
milton_persist_set_blocks_for_painting(Milton* milton)
{
    // Set up blocks.
    MiltonPersist* p = milton->persist;

    reset(&p->blocks);

    // Fixed blocks
    push(&p->blocks, { Block_COLOR_PICKER });
    push(&p->blocks, { Block_BRUSHES });
    push(&p->blocks, { Block_BUTTONS });

    // Movable blocks
    push(&p->blocks, { Block_PAINTING_DESCRIPTION });

    for (Layer* layer = milton->canvas->root_layer;
         layer;
         layer = layer->next) {
        SaveBlockHeader header = {};
        header.type = Block_LAYER_CONTENT;
        header.block_layer.id = layer->id;
        SaveBlock block = {};
        block.header = header;
        block.dirty = true;
        push(&p->blocks,  block);
    }

    for (sz i = 0; i < p->blocks.count; ++i) {
        p->blocks[i].dirty = true;
    }
}

#define END_IF_FAILED(expr)\
    do {\
        failure = expr;\
        if (failure) { \
            goto END; \
        }\
    } while (0)

#define FAIL(msg) \
    failure = msg; \
    goto END;

#define WRITE(address, sz, num, fd) \
        do { \
            if (!fwrite_checked(address, sz, num, fd)) { \
                static char* msg = #address;\
                failure = msg;\
                goto END; \
            }  \
            debug_mark_bytes(num * sz); \
        } while(0);

static char*
save_block_brushes(Milton* milton, FILE* fd)
{
    char* failure = NULL;

    u16 num_brushes = 3;  // Brush, eraser, primitive.
    WRITE(&num_brushes, sizeof(num_brushes), 1, fd);
    WRITE(&milton->brushes, sizeof(Brush), num_brushes, fd);
    WRITE(&milton->brush_sizes, sizeof(i32), num_brushes, fd);
END:
    return failure;
}

static char*
save_block_buttons(Milton* milton, FILE* fd)
{
    char* failure = NULL;

    MiltonGui* gui = milton->gui;
    // Count buttons
    i32 button_count = 0;
    for (ColorButton* b = gui->picker.color_buttons; b!= NULL; b = b->next) { button_count++; }
    // Write
    WRITE(&button_count, sizeof(i32), 1, fd);
    for ( ColorButton* b = gui->picker.color_buttons;
          b!= NULL;
          b = b->next ) {
        WRITE(&b->rgba, sizeof(v4f), 1, fd);
    }
END:
    return failure;
}

static char*
save_block_color_picker(Milton* milton, FILE* fd)
{
    char* failure = NULL;

    v3f rgb = gui_get_picker_rgb(milton->gui);
    WRITE(&rgb, sizeof(rgb), 1, fd);

END:
    return failure;
}

static char*
save_block_painting_description(Milton* milton, FILE* fd)
{
    char* failure = NULL;
    i32 num_layers = 0;
    Layer* layer;
    // TODO: hard code struct sizes used by file format
    u32 size_of_canvas_view = sizeof (CanvasView);
    WRITE(&size_of_canvas_view, sizeof (u32), 1, fd);
    WRITE(milton->view, sizeof(CanvasView), 1, fd);

    WRITE(&milton->canvas->layer_guid, sizeof(i32), 1, fd);

    num_layers = layer::number_of_layers(milton->canvas->root_layer);
    WRITE(&num_layers, sizeof (decltype(num_layers)), 1, fd);
    WRITE(&milton->canvas->layer_guid, sizeof(i32), 1, fd);

    layer = milton->canvas->root_layer;
    for (i32 layer_i = 0 ; layer_i < num_layers; ++layer_i) {
        mlt_assert(layer);
        i32 num_effects = 0;
        for ( LayerEffect* e = layer->effects; e != NULL; e = e->next ) { ++num_effects; }

        WRITE(&layer->id, sizeof (i32), 1, fd);

        WRITE(&layer->flags, sizeof (i32), 1, fd);

        f32 alpha = layer->alpha;
        WRITE(&alpha, sizeof (f32), 1, fd);

        WRITE(&num_effects, sizeof (i32), 1, fd);

        for ( LayerEffect* e = layer->effects; e != NULL; e = e->next ) {
            WRITE(&e->type, sizeof(e->type), 1, fd);
            WRITE(&e->enabled, sizeof(e->enabled), 1, fd);
            switch (e->type) {
                case LayerEffectType_BLUR: {
                    WRITE(&e->blur.original_scale, sizeof(e->blur.original_scale), 1, fd);
                    WRITE(&e->blur.kernel_size, sizeof(e->blur.kernel_size), 1, fd);
                } break;
                default : {
                    mlt_assert("Leyer effect save not implemented.");
                } break;
            }
        }

        layer = layer->next;
    }
END:
    return failure;
}

static b32
block_equals(SaveBlockHeader a, SaveBlockHeader b)
{
    b32 eq = false;
    if (a.type == b.type) {
        if (a.type == Block_LAYER_CONTENT) {
            eq = a.block_layer.id == b.block_layer.id;
        }
        else {
            eq = true;
        }
    }
    return eq;
}

void
milton_mark_block_for_save(MiltonPersist* p, SaveBlockHeader header)
{
    u64 count = p->blocks.count;
    for (sz block_i = 0; block_i < count; ++block_i) {
        if (block_equals(header, p->blocks[block_i].header)) {
            p->blocks[block_i].dirty = true;

            // Swap
            if (header.type >= NUM_FIXED_BLOCKS && block_i != count-1) {
                SaveBlockHeader tmp = p->blocks[count - 1].header;

                p->blocks[count - 1] = {};
                p->blocks[count - 1].header = p->blocks[block_i].header;
                p->blocks[count - 1].dirty = true;

                p->blocks[block_i] = {};
                p->blocks[block_i].header = tmp;
            }
            break;
        }
    }
}

static char*
write_stroke_list(FILE* fd, StrokeList* strokes, u64 stroke_i)
{
    char* failure = NULL;
    StrokeIterator stroke_iter = {};

    for (Stroke* stroke = stroke_iter_init_at(strokes, &stroke_iter, stroke_i);
         stroke != NULL;
         stroke = stroke_iter_next(&stroke_iter)) {

        WRITE(&stroke->brush, sizeof (Brush), 1, fd);
        WRITE(&stroke->num_points, sizeof (i32), 1, fd);

        // TODO: Is this really slow?
        for (i32 point_i = 0; point_i < stroke->num_points; ++point_i) {
            PersistStrokePoint point /*={}*/;
            point.point = stroke->points[point_i];
            point.pressure = stroke->pressures[point_i];
            WRITE(&point, sizeof (PersistStrokePoint), 1, fd);
        }
    }
END:
    return failure;
}

static char*
save_block_layer_content(Milton* milton, FILE* fd, i32 layer_id)
{
    char* failure = NULL;

    Layer* layer = layer::get_by_id(milton->canvas->root_layer, layer_id);
    Stroke* stroke = NULL;

    i32 num_strokes = count(&layer->strokes);
    WRITE(&num_strokes, sizeof (i32), 1, fd);

    failure = write_stroke_list(fd, &layer->strokes, 0);
END:
    return failure;
}

static char*
save_layer_content_increment(Milton* milton, size_t bytes_end, FILE* fd, i32 layer_id, u64 stroke_i)
{
    char* failure = NULL;
    Layer* l = layer::get_by_id(milton->canvas->root_layer, layer_id);

    fseek(fd, sizeof(SaveBlockHeader), SEEK_CUR);  // Skip to num_strokes

    i32 num_strokes = count(&l->strokes);  // Always update num_strokes.
    WRITE(&num_strokes, sizeof (i32), 1, fd);

    fseek(fd, bytes_end, SEEK_SET);


    failure = write_stroke_list(fd, &l->strokes, stroke_i);
END:
    return failure;
}

static char*
save_block(Milton* milton, FILE* fd, SaveBlockHeader* header)
{
    char* failure = NULL;
    WRITE(header, sizeof (SaveBlockHeader), 1, fd);

    switch(header->type) {
        case Block_BRUSHES: {
            failure = save_block_brushes(milton, fd);
        } break;
        case Block_BUTTONS: {
            failure = save_block_buttons(milton, fd);
        } break;
        case Block_COLOR_PICKER: {
            failure = save_block_color_picker(milton, fd);
        } break;
        case Block_PAINTING_DESCRIPTION: {
            failure = save_block_painting_description(milton, fd);
        } break;
        case Block_LAYER_CONTENT: {
            failure = save_block_layer_content(milton, fd, header->block_layer.id);
        } break;
        default: {
            mlt_assert(!"block dispatch");
        } break;
    }
END:
    return failure;
}

static char*
save_block_list(Milton* milton, FILE* fd)
{
    char* failure = NULL;
    MiltonPersist* p = milton->persist;

    for ( u64 block_index = 0; block_index < NUM_FIXED_BLOCKS; ++block_index ) {
        SaveBlock* block = &p->blocks[block_index];
        SaveBlockHeader* h = &block->header;

        if ( block->dirty ) {
            mlt_assert(block->bytes_begin == 0 || block->bytes_begin == ftell(fd));
            block->bytes_begin = ftell(fd);
            char* block_fail = save_block(milton, fd, h);
            mlt_assert(block->bytes_end == 0 || block->bytes_end == ftell(fd));
            block->bytes_end = ftell(fd);

            if ( block_fail ) {
                failure = block_fail;
                break;
            }

            block->dirty = false;

        }
        else {
            fseek(fd, block->bytes_end, SEEK_SET);
        }
    }

    b32 file_dirty = false;  // When a dynamic block is dirty, the rest of the file is dirty.
    for ( u64 block_index = NUM_FIXED_BLOCKS; block_index < p->blocks.count; ++block_index ) {
        SaveBlock* block = &p->blocks[block_index];
        SaveBlockHeader* h = &block->header;
        u64 current_bytes = ftell(fd);


        if ( !block->dirty && !file_dirty ) {
            mlt_assert (block->bytes_begin == current_bytes);
            fseek(fd, block->bytes_end, SEEK_SET);
        }
        else {
            file_dirty = true;

            // Incremental layer content
            if ( block_index == p->blocks.count - 1 &&
                 block->header.type == Block_LAYER_CONTENT &&
                 block->bytes_begin < block->bytes_end ) {
                block->bytes_begin = ftell(fd);

                failure = save_layer_content_increment(milton, block->bytes_end, fd, block->header.block_layer.id, block->num_saved_strokes);

                block->bytes_end = ftell(fd);
            }
            // Regular block save
            else {
                block->bytes_begin = ftell(fd);
                failure = save_block(milton, fd, &block->header);
                block->bytes_end = ftell(fd);
                if ( !failure ) {
                    block->dirty = false;
                }
            }

            if ( block->header.type == Block_LAYER_CONTENT ) {
                Layer* l = layer::get_by_id(milton->canvas->root_layer, block->header.block_layer.id);
                block->num_saved_strokes = layer::count_strokes(l);
            }
        }
    }


    return failure;
}

#define READ(address, size, num, fd) \
        do { \
            if (!fread_checked(address,size,num,fd))  { \
                static char* msg = #address;\
                failure = msg;\
                goto END;\
            } \
        } while(0)

static char*
read_block_brushes(Milton* milton, FILE* fd)
{
    char* failure = NULL;

    u16 num_brushes = 3;  // Brush, eraser, primitive.
    READ(&num_brushes, sizeof(num_brushes), 1, fd);

    if (num_brushes != 3) {
        FAIL("invalid number of brushes");
    }

    READ(&milton->brushes, sizeof(Brush), num_brushes, fd);
    READ(&milton->brush_sizes, sizeof(i32), num_brushes, fd);
END:
    return failure;
}

static char*
read_block_color_picker(Milton* milton, FILE* fd)
{
    char* failure = NULL;

    v3f rgb = {};
    READ(&rgb, sizeof(rgb), 1, fd);
    gui_picker_from_rgb(&milton->gui->picker, rgb);

END:
    return failure;
}


static char*
read_block_buttons(Milton* milton, FILE* fd)
{
    char* failure = NULL;

    MiltonGui* gui = milton->gui;
    // Count buttons
    i32 button_count = 0;
    for (ColorButton* b = gui->picker.color_buttons; b!= NULL; b = b->next) { button_count++; }

    i32 button_count_in_file = 0;
    READ(&button_count_in_file, sizeof(i32), 1, fd);

    if (button_count_in_file != button_count) {
        FAIL("Unexpected number of buttons in file");
    }
    for ( ColorButton* b = gui->picker.color_buttons;
          b!= NULL;
          b = b->next ) {
        READ(&b->rgba, sizeof(v4f), 1, fd);
    }
END:
    return failure;
}

static char*
read_block_painting_description(Milton* milton, FILE* fd)
{
    char* failure = NULL;
    CanvasState* canvas = milton->canvas;
    i32 working_layer_id = 0;
    i32 layer_guid = 0;
    i32 num_layers = 0;

    u32 size_of_canvas_view = 0;
    READ(&size_of_canvas_view, sizeof (u32), 1, fd);
    if (size_of_canvas_view != sizeof (CanvasView)) {
        failure = "Unexpected size of CanvasView";
        goto END;
    }
    else {
        READ(milton->view, sizeof(CanvasView), 1, fd);
    }

    // This is going to get rewritten when creating layers. Save it and rewrite after.
    working_layer_id = milton->view->working_layer_id;

    READ(&milton->canvas->layer_guid, sizeof(i32), 1, fd);

    READ(&num_layers, sizeof (decltype(num_layers)), 1, fd);

    READ(&layer_guid, sizeof(i32), 1, fd);

    for (i32 layer_i = 0 ; layer_i < num_layers; ++layer_i) {
        i32 id = 0;
        READ(&id, sizeof (i32), 1, fd);

        milton_new_layer_with_id(milton, id);
        Layer* layer = milton->canvas->working_layer;
        mlt_assert(layer);

        READ(&layer->flags, sizeof (i32), 1, fd);

        READ(&layer->alpha, sizeof (f32), 1, fd);

        i32 num_effects = 0;

        READ(&num_effects, sizeof (i32), 1, fd);
        if ( num_effects > 0 ) {
            LayerEffect** e = &layer->effects;
            for ( i64 i = 0; i < num_effects; ++i ) {
                mlt_assert(*e == NULL);
                *e = arena_alloc_elem(&canvas->arena, LayerEffect);
                READ(&(*e)->type, sizeof((*e)->type), 1, fd);
                READ(&(*e)->enabled, sizeof((*e)->enabled), 1, fd);
                switch ((*e)->type) {
                    case LayerEffectType_BLUR: {
                        READ(&(*e)->blur.original_scale, sizeof((*e)->blur.original_scale), 1, fd);
                        READ(&(*e)->blur.kernel_size, sizeof((*e)->blur.kernel_size), 1, fd);
                    } break;
                }
                e = &(*e)->next;
            }
        }
    }

    milton->view->working_layer_id = working_layer_id;

    milton->canvas->layer_guid = layer_guid;

END:
    return failure;
}

static char*
read_block_layer_content(Milton* milton, FILE* fd, i32 layer_id)
{
    char* failure = NULL;
    CanvasState* canvas = milton->canvas;

    Layer* layer = layer::get_by_id(milton->canvas->root_layer, layer_id);

    mlt_assert(count(&layer->strokes) == 0);

    Stroke* stroke = NULL;

    i32 num_strokes = 0;
    READ(&num_strokes, sizeof (i32), 1, fd);

    for (i32 stroke_i = 0; stroke_i < num_strokes; ++stroke_i) {
        Stroke stroke = {};
        READ(&stroke.brush, sizeof(Brush), 1, fd);
        READ(&stroke.num_points, sizeof(i32), 1, fd);

        stroke.points = arena_alloc_array(&canvas->arena, stroke.num_points, v2l);
        stroke.pressures = arena_alloc_array(&canvas->arena, stroke.num_points, f32);

        for (i32 point_i = 0; point_i < stroke.num_points; ++point_i) {
            PersistStrokePoint point = {};
            READ(&point, sizeof (PersistStrokePoint), 1, fd);

            stroke.points[point_i] = point.point;
            stroke.pressures[point_i] = point.pressure;

        }
        stroke.layer_id = layer_id;
        stroke.bounding_rect = bounding_box_for_stroke(&stroke);
        layer::layer_push_stroke(layer, stroke);
    }

END:
    return failure;
}


static char*
read_block_list(Milton* milton, u32 block_count, FILE* fd)
{
    char* failure = NULL;
    MiltonPersist* p = milton->persist;
    mlt_assert(p->blocks.count == 0);

    for (u32 block_i = 0; block_i < block_count; ++block_i) {
        u64 bytes_begin = ftell(fd);

        SaveBlockHeader header = {};
        READ(&header, sizeof (SaveBlockHeader), 1, fd);

        switch (header.type) {
            case Block_PAINTING_DESCRIPTION: {
                END_IF_FAILED ( read_block_painting_description(milton, fd) );
            } break;
            case Block_LAYER_CONTENT: {
                END_IF_FAILED ( read_block_layer_content(milton, fd, header.block_layer.id) );
            } break;
            case Block_COLOR_PICKER: {
                END_IF_FAILED ( read_block_color_picker(milton, fd) );
            } break;
            case Block_BUTTONS: {
                END_IF_FAILED ( read_block_buttons(milton, fd) );
            } break;
            case Block_BRUSHES: {
                END_IF_FAILED ( read_block_brushes(milton, fd) );
            } break;
            default: {
                failure = "Invalid block identifier.";
                goto END;
            }
        }
        SaveBlock block = {};
        block.header = header;
        block.bytes_begin = bytes_begin;
        block.bytes_end = ftell(fd);
        push(&p->blocks, block);
    }

END:
    return failure;
}

b32
can_save_incrementally(u16 save_id, DArray<SaveBlock>* blocks)
{
    b32 result = false;
    return result;
}

void
milton_save_v6(Milton* milton)
{
    milton_save_v6_file(milton, milton->persist->mlt_file_path);
}

void
milton_save_v6_file(Milton* milton, PATH_CHAR* fname)
{
    #if MILTON_DEBUG_SAVE
    g_bytes_written = 0;
    #endif

    // Declaring variables here to silence compiler warnings about GOTO jumping declarations.
    // TODO: is this still needed?
    i32 history_count = 0;
    u32 milton_binary_version = 0;
    i32 num_layers = 0;
    MiltonPersist* p = milton->persist;

    milton->flags |= MiltonStateFlags_LAST_SAVE_FAILED;  // Assume failure. Remove flag on success.

    int pid = (int)getpid();
    PATH_CHAR tmp_fname[MAX_PATH] = {};
    PATH_SNPRINTF(tmp_fname, MAX_PATH, TO_PATH_STR("milton_tmp.%d.mlt"), pid);

    platform_fname_at_config(tmp_fname, MAX_PATH);

    b32 do_incremental_save = true;

	FILE* fd = NULL;

    char* failure = NULL;

    // Move existing file to temp location.
    // TODO: Maybe warn that doing this across drive letter might result in copying all the data?
    // TODO: Maybe the temp location should be right next to the file in question?
    if ( !platform_move_file(fname, tmp_fname) ) {
        failure = "could not move file to temp location for incremental save.";
    }

    fd = platform_fopen(tmp_fname, TO_PATH_STR("r+b"));

    if ( !fd ) {
        fd = platform_fopen(tmp_fname, TO_PATH_STR("w"));
        // TODO: Mark all blocks as dirty to prevent trying to save incrementally to a new file?
    }

    if ( fd ) {
        // TODO: do not always save boiler plate.

        u32 milton_magic = MILTON_MAGIC_NUMBER;

        WRITE(&milton_magic, sizeof(u32), 1, fd);

        milton_binary_version = milton->persist->mlt_binary_version;

        WRITE(&milton_binary_version, sizeof(u32), 1, fd);

        u16 block_size = (u16)sizeof(SaveBlockHeader);
        WRITE(&block_size, sizeof (u16), 1, fd);

        u32 block_count = (u32)milton->persist->blocks.count;
        WRITE(&block_count, sizeof(u32), 1, fd);

        // Block:
        failure = save_block_list(milton, fd);
    }
END:

    if (!fd) {
        static char msg[MAX_PATH] = {};
        snprintf(msg, MAX_PATH, "could not open file. errno %d", errno);

        failure = msg;
    }
    else {
        int file_error = ferror(fd);
        if ( file_error == 0 ) {
            int close_ret = fclose(fd);
        }
        else {
            static char msg[MAX_PATH] = {};
            snprintf(msg, MAX_PATH, "file error %d", file_error);
            failure = msg;
        }
    }

    if ( failure ) {
        milton_log("FAILED SAVE: [%s]\n", failure);
    }
    else {
        if ( !platform_move_file(tmp_fname, fname) ) {
            milton_log("Could not replace filename in atomic save: [%s]\n", fname);
            failure = "Unsuccessful atomic save";
        }
        else {
            // Success!
            milton_save_postlude(milton);

            p->save_id++;
            if (p->save_id == 0) {
                p->save_id++;
            }
        }
    }
    if (failure) {
        char message[MAX_PATH] = {};
        snprintf(message, array_count(message), "Milton save failed: %s.", failure);
        platform_dialog(message, "Error");
    }

#undef WRITE

    save_debug_log("End of save: %d bytes written.", g_bytes_written);
}

void
milton_load_v6(Milton* milton)
{
    milton_load_v6_file(milton, milton->persist->mlt_file_path);
}

void
milton_load_v6_file(Milton* milton, PATH_CHAR* fname)
{
    char* failure = NULL;

    FILE* fd = platform_fopen(fname, TO_PATH_STR("rb"));

    if ( !fd ) {
        failure = "could not open file";
    }
    else {
        milton_reset_canvas(milton);
        gpu_free_strokes(milton->render_data, milton->canvas);  // TODO: Probably do this in just_loaded handler

        u32 milton_magic = 0;
        u32 milton_binary_version = 0;
        READ(&milton_magic, sizeof(u32), 1, fd);

        if ( milton_magic != MILTON_MAGIC_NUMBER ) {
            failure = "wrong magic number";
			goto END;
        }

        READ(&milton_binary_version, sizeof(u32), 1, fd);

        if (milton_binary_version > MILTON_MINOR_VERSION) {
            failure = "This file was created by a newer version of Milton";
            goto END;
        }

        mlt_assert(milton_binary_version >= 6);

        milton->persist->blocks.count = 0;

        u16 block_size = 0;
        READ(&block_size, sizeof (u16), 1, fd);

        if (block_size != sizeof (SaveBlockHeader)) {
            failure = "Invalid size for block header";
            goto END;
        }

        u32 block_count = 0;
        READ(&block_count, sizeof(u32), 1, fd);

        failure = read_block_list(milton, block_count, fd);
    }
END:
    if (fd) {
        int close_ret = fclose(fd);
        if (close_ret) {
            failure = "Could not close file";
        }
    }

    if (failure) {
        milton_log("File load error: [%s]\n", failure);
        milton_reset_canvas_and_set_default(milton);
    }
    else {
        milton->flags |= MiltonStateFlags_JUST_SAVED;
    }
}
#undef READ


void
milton_save_preV6(Milton* milton)
{
    // Declaring variables here to silence compiler warnings about GOTO jumping declarations.
    i32 history_count = 0;
    u32 milton_binary_version = 0;
    i32 num_layers = 0;
    milton->flags |= MiltonStateFlags_LAST_SAVE_FAILED;  // Assume failure. Remove flag on success.

    int pid = (int)getpid();
    PATH_CHAR tmp_fname[MAX_PATH] = {};
    PATH_SNPRINTF(tmp_fname, MAX_PATH, TO_PATH_STR("milton_tmp.%d.mlt"), pid);

    platform_fname_at_config(tmp_fname, MAX_PATH);

    FILE* fd = platform_fopen(tmp_fname, TO_PATH_STR("wb"));

    b32 ok = true;

    if ( fd ) {
#define WRITE(address, sz, num, fd) do { ok = fwrite_checked(address, sz, num, fd); if (!ok) { goto END; }  } while(0)
        u32 milton_magic = MILTON_MAGIC_NUMBER;

        WRITE(&milton_magic, sizeof(u32), 1, fd);

        milton_binary_version = milton->persist->mlt_binary_version;

        WRITE(&milton_binary_version, sizeof(u32), 1, fd);
        WRITE(milton->view, sizeof(CanvasView), 1, fd);

        num_layers = layer::number_of_layers(milton->canvas->root_layer);
        WRITE(&num_layers, sizeof(i32), 1, fd);
        WRITE(&milton->canvas->layer_guid, sizeof(i32), 1, fd);

        for ( Layer* layer = milton->canvas->root_layer; layer; layer=layer->next  ) {
            if ( layer->strokes.count > INT_MAX ) {
                milton_die_gracefully("FATAL. Number of strokes in layer greater than can be stored in file format. ");
            }
            i32 num_strokes = (i32)layer->strokes.count;
            char* name = layer->name;
            i32 len = (i32)(strlen(name) + 1);
            WRITE(&len, sizeof(i32), 1, fd);
            WRITE(name, sizeof(char), (size_t)len, fd);
            WRITE(&layer->id, sizeof(i32), 1, fd);
            WRITE(&layer->flags, sizeof(layer->flags), 1, fd);
            WRITE(&num_strokes, sizeof(i32), 1, fd);
            if ( ok ) {
                for ( i32 stroke_i = 0; ok && stroke_i < num_strokes; ++stroke_i ) {
                    Stroke* stroke = get(&layer->strokes, stroke_i);
                    mlt_assert(stroke->num_points > 0);
                    if (stroke->num_points > 0 && stroke->num_points <= STROKE_MAX_POINTS) {
                        WRITE(&stroke->brush, sizeof(Brush), 1, fd);
                        WRITE(&stroke->num_points, sizeof(i32), 1, fd);
                        WRITE(stroke->points, sizeof(v2l), (size_t)stroke->num_points, fd);
                        WRITE(stroke->pressures, sizeof(f32), (size_t)stroke->num_points, fd);
                        WRITE(&stroke->layer_id, sizeof(i32), 1, fd);
                        if ( !ok ) {
                            break;
                        }
                    } else {
                        milton_log("WARNING: Trying to write a stroke of size %d\n", stroke->num_points);
                    }

                }
            } else {
                ok = false;
            }
            {
                i64 num_effects = 0;
                for ( LayerEffect* e = layer->effects; e != NULL; e = e->next ) {
                    ++num_effects;
                }
                WRITE(&num_effects, sizeof(num_effects), 1, fd);
                for ( LayerEffect* e = layer->effects; e != NULL; e = e->next ) {
                    WRITE(&e->type, sizeof(e->type), 1, fd);
                    WRITE(&e->enabled, sizeof(e->enabled), 1, fd);
                    switch (e->type) {
                        case LayerEffectType_BLUR: {
                            WRITE(&e->blur.original_scale, sizeof(e->blur.original_scale), 1, fd);
                            WRITE(&e->blur.kernel_size, sizeof(e->blur.kernel_size), 1, fd);
                        } break;
                    }
                }
            }
        }

        if ( milton_binary_version >= 5 ) {
           v3f rgb = gui_get_picker_rgb(milton->gui);
           WRITE(&rgb, sizeof(rgb), 1, fd);
        }
        else {
           WRITE(&milton->gui->picker.data, sizeof(PickerData), 1, fd);
        }

        // Buttons
        if ( ok ) {
            i32 button_count = 0;
            MiltonGui* gui = milton->gui;
            // Count buttons
            for (ColorButton* b = gui->picker.color_buttons; b!= NULL; b = b->next, button_count++) { }
            // Write
            WRITE(&button_count, sizeof(i32), 1, fd);
            if ( ok ) {
                for ( ColorButton* b = gui->picker.color_buttons;
                      ok && b!= NULL;
                      b = b->next ) {
                    WRITE(&b->rgba, sizeof(v4f), 1, fd);
                }
            }
        }

        // Brush
        if ( milton_binary_version >= 2 && milton_binary_version <= 5 ) {
            // PEN, ERASER
            WRITE(&milton->brushes, sizeof(Brush), 2, fd);
            // Sizes
            WRITE(&milton->brush_sizes, sizeof(i32), 2, fd);
        }
        else if ( milton_binary_version > 5 ) {
            u16 num_brushes = 3;  // Brush, eraser, primitive.
            WRITE(&num_brushes, sizeof(num_brushes), 1, fd);
            WRITE(&milton->brushes, sizeof(Brush), num_brushes, fd);
            WRITE(&milton->brush_sizes, sizeof(i32), num_brushes, fd);
        }

        history_count = (i32)milton->canvas->history.count;
        if ( milton->canvas->history.count > INT_MAX ) {
            history_count = 0;
        }
        WRITE(&history_count, sizeof(history_count), 1, fd);
        WRITE(milton->canvas->history.data, sizeof(*milton->canvas->history.data), (size_t)history_count, fd);

        // MLT 3
        // Layer alpha
        if ( milton_binary_version >= 3 ) {
            Layer* l = milton->canvas->root_layer;
            for ( i64 i = 0; ok && i < num_layers; ++i ) {
                mlt_assert(l);
                WRITE(&l->alpha, sizeof(l->alpha), 1, fd);
                l = l->next;
            }
        }
END:
        int file_error = ferror(fd);
        if ( file_error == 0 ) {
            int close_ret = fclose(fd);
            if ( close_ret == 0 ) {
                ok = platform_move_file(tmp_fname, milton->persist->mlt_file_path);
                if ( ok ) {
                    //  \o/
                    milton_save_postlude(milton);
                }
                else {
                    milton_log("Could not move file. Moving on. Avoiding this save.\n");
                    milton->flags |= MiltonStateFlags_MOVE_FILE_FAILED;
                }
            }
            else {
                milton_log("File error when closing handle. Error code %d. \n", close_ret);
            }
        }
        else {
            milton_log("File IO error. Error code %d. \n", file_error);
        }

    }
    else {
        milton_die_gracefully("Could not create file for saving! ");
        return;
    }
#undef WRITE
}

void
milton_save(Milton* milton)
{
    if ( milton->persist->DEV_use_new_format_write ) {
        milton_save_v6(milton);
    }
    else {
        milton_save_preV6(milton);
    }
}

PATH_CHAR*
milton_get_last_canvas_fname()
{
    PATH_CHAR* last_fname = (PATH_CHAR*)mlt_calloc(MAX_PATH, sizeof(PATH_CHAR), "Strings");

    PATH_CHAR full[MAX_PATH] = {};

    PATH_STRCPY(full, TO_PATH_STR("saved_path"));
    platform_fname_at_config(full, MAX_PATH);
    FILE* fd = platform_fopen(full, TO_PATH_STR("rb+"));

    if ( fd ) {
        u64 len = 0;
        fread(&len, sizeof(len), 1, fd);
        if ( len < MAX_PATH ) {
            fread(last_fname, sizeof(PATH_CHAR), len, fd);
            // If the read fails, or if the file doesn't exist, milton_load
            // will fail gracefully and load a default canvas.
            fclose(fd);
        }
    } else {
        mlt_free(last_fname, "Strings");
    }

    return last_fname;

}

void
milton_set_last_canvas_fname(PATH_CHAR* last_fname)
{
    //PATH_CHAR* full = (PATH_CHAR*)mlt_calloc(MAX_PATH, sizeof(char));
    //wcscpy(full, "last_canvas_fname");
    PATH_CHAR full[MAX_PATH] = { TO_PATH_STR("saved_path") };
    platform_fname_at_config(full, MAX_PATH);
    FILE* fd = platform_fopen(full, TO_PATH_STR("wb"));
    if ( fd ) {
        u64 len = PATH_STRLEN(last_fname)+1;
        fwrite(&len, sizeof(len), 1, fd);
        fwrite(last_fname, sizeof(*last_fname), len, fd);
        fclose(fd);
    }
}

// Called by stb_image
static void
write_func(void* context, void* data, int size)
{
    FILE* fd = *(FILE**)context;

    if ( fd ) {
        size_t written = fwrite(data, (size_t)size, 1, fd);
        if ( written != 1 ) {
            fclose(fd);
            *(FILE**)context = NULL;
        }
    }
}

void
milton_save_buffer_to_file(PATH_CHAR* fname, u8* buffer, i32 w, i32 h)
{
    int len = 0;
    {
        size_t sz = PATH_STRLEN(fname);
        if ( sz > ((1u << 31) -1) ) {
            milton_die_gracefully("A really, really long file name. This shouldn't happen.");
        }
        len = (int)sz;
    }
    size_t ext_sz = ( len+1 ) * sizeof(PATH_CHAR);
    PATH_CHAR* fname_copy = (PATH_CHAR*)mlt_calloc(ext_sz, 1, "Strings");
    fname_copy[0] = '\0';
    PATH_STRCPY(fname_copy, fname);

    // NOTE: This should work with unicode.
    int ext_len = 0;
    PATH_CHAR* ext = fname_copy + len;
    b32 found = false;
    {
        int safety = len;
        while ( *--ext != '.' ) {
            if( safety-- == 0 ) {
                break;
            }
        }
        if ( safety > 0 ) {
            found = true;
            ext_len = len - safety;
            ++ext;
        }
    }

    if ( found ) {
        for ( int i = 0; i < ext_len; ++i ) {
            PATH_CHAR c = ext[i];
            ext[i] = PATH_TOLOWER(c);
        }

        FILE* fd = NULL;

        fd = platform_fopen(fname, TO_PATH_STR("wb"));

        if ( fd ) {
            if ( !PATH_STRCMP(ext, TO_PATH_STR("png")) ) {
                stbi_write_png_to_func(write_func, &fd, w, h, 4, buffer, 0);
            }
            else if ( !PATH_STRCMP(ext, TO_PATH_STR("jpg")) || !PATH_STRCMP(ext, TO_PATH_STR("jpeg")) ) {
                tje_encode_with_func(write_func, &fd, 3, w, h, 4, buffer);
            }
            else {
                platform_dialog("File extension not handled by Milton\n", "Info");
            }

            // !! fd might have been set to NULL if write_func failed.
            if ( fd ) {
                if ( ferror(fd) ) {
                    platform_dialog("Unknown error when writing to file :(", "Unknown error");
                }
                else {
                    platform_dialog("Image exported successfully!", "Success");
                }
                fclose(fd);
            }
            else {
                platform_dialog("File created, but there was an error writing to it.", "Error");
            }
        }
        else {
            platform_dialog ( "Could not open file", "Error" );
        }
    }
    else {
        platform_dialog("File name missing extension!\n", "Error");
    }
    mlt_free(fname_copy, "Strings");
}

b32
milton_appstate_load(PlatformPrefs* prefs)
{
    b32 loaded = false;
    PATH_CHAR fname[MAX_PATH] = TO_PATH_STR("PREFS.milton_prefs");
    platform_fname_at_config(fname, MAX_PATH);

    milton_log("Prefs file: %s\n", fname);

    FILE* fd = platform_fopen(fname, TO_PATH_STR("rb"));

    if ( fd ) {
        if ( !ferror(fd) ) {
            fread(&prefs->width, sizeof(i32), 1, fd);
            fread(&prefs->height, sizeof(i32), 1, fd);
            loaded = true;
        }
        else {
            milton_log("Error writing to prefs file...\n");
        }
        fclose(fd);
    }
    else {
        milton_log("Could not open file for writing prefs\n");
    }

    return loaded;
}

void
milton_appstate_save(PlatformPrefs* prefs)
{
    PATH_CHAR fname[MAX_PATH] = TO_PATH_STR("PREFS.milton_prefs");
    platform_fname_at_config(fname, MAX_PATH);
    FILE* fd = platform_fopen(fname, TO_PATH_STR("wb"));
    if ( fd ) {
        if ( !ferror(fd) ) {
            fwrite(&prefs->width, sizeof(i32), 1, fd);
            fwrite(&prefs->height, sizeof(i32), 1, fd);
        }
        else {
            milton_log( "Error writing to profs file...\n" );
        }
        fclose(fd);
    }
    else {
        milton_log("Could not open file for writing prefs :(\n");
    }
}

void milton_settings_load(MiltonSettings* settings)
{
    PATH_CHAR settings_fname[MAX_PATH] = TO_PATH_STR("milton_settings.ini"); {
        platform_fname_at_config(settings_fname, MAX_PATH);
    }

    b32 ok = false;
    auto* fd = platform_fopen(settings_fname, TO_PATH_STR("rb"));
    if ( fd ) {
        u16 struct_size = 0;
        if ( fread(&struct_size, sizeof(u16), 1, fd) ) {
            if (struct_size <= sizeof(*settings)) {
                if ( fread(settings, sizeof(*settings), 1, fd) ) {
                    ok = true;
                }
            }
        }
    }
    if ( !ok ) {
        milton_log("Warning: Failed to read settings file\n");
    }
}

void milton_settings_save(MiltonSettings* settings)
{
    PATH_CHAR settings_fname[MAX_PATH] = TO_PATH_STR("milton_settings.ini"); {
        platform_fname_at_config(settings_fname, MAX_PATH);
    }

    b32 ok = false;
    auto* fd = platform_fopen(settings_fname, TO_PATH_STR("wb"));
    if ( fd ) {
        mlt_assert( sizeof(*settings) < 1<<16 );
        u16 sz = sizeof(*settings);
        if ( fwrite(&sz, sizeof(sz), 1, fd) ) {
            if ( fwrite(settings, sizeof(*settings), 1, fd) ) {
                ok = true;
            }
        }
    }
    if ( !ok ) {
        milton_log("Warning: could not correctly save settings file\n");
    }
}
