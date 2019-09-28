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

static u64 g_bytes_written = 0;

void save_debug_log(char* message, ...)
{
    va_list args;
    va_start(args, message);
    milton_log_args(message, args);
    va_end(args);
}

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

void
milton_unset_last_canvas_fname()
{
    b32 del = platform_delete_file_at_config(TO_PATH_STR("saved_path"), DeleteErrorTolerance_OK_NOT_EXIST);
    if ( del == false ) {
        platform_dialog("The default canvas could not be set to open the next time you run Milton. Please contact the developers.", "Important");
    }
}

static b32
read_brushes(Brush* brushes, i32 num_brushes, FILE* fd)
{
    i32 size = 0;
    b32 ok = fread_checked(&size, sizeof(size), 1, fd);

    if (size == 0 || size > sizeof(Brush)) {
        ok = false;
    }

    for (i32 i = 0; i < num_brushes; ++i) {
        brushes[i] = default_brush();
        if (ok) { ok = fread_checked(brushes + i, size, 1, fd); }
    }

    return ok;
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
    gpu_free_strokes(milton->renderer, milton->canvas);
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
            if ( milton_binary_version < MILTON_MINOR_VERSION ) {
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

        if ( milton_binary_version >= 9 ) {
            // Defaults
            *milton->view = {};

            READ(&milton->view->size, sizeof(u32), 1, fd); // Read size.
            if (milton->view->size > sizeof(CanvasView)) {
                ok = false;
                handled = true;
                goto END;
            }
            READ((u8*)milton->view + offsetof(CanvasView, screen_size),
                milton->view->size - sizeof(u32),
                1,
                fd);  // Rest of the struct.

            milton->view->size = sizeof(CanvasView);
        }
        else if ( milton_binary_version >= 4 && milton_binary_version < 9) {
            init_view(milton->view, milton->settings->background_color, milton->view->screen_size.x, milton->view->screen_size.y); // defaults

            size_t bytes_offset = offsetof(CanvasView, screen_size);

            READ((u8*)milton->view + bytes_offset, sizeof(CanvasViewPreV9), 1, fd);

            // Patch angle, which was stomped by the old num_layers member, which we don't use anymore.
            milton->view->angle = 0.0f;
        } else {
            CanvasViewPreV4 legacy_view = {};
            READ(&legacy_view, sizeof(CanvasViewPreV4), 1, fd);
            milton->view->screen_size = legacy_view.screen_size;
            milton->view->scale = legacy_view.scale;
            milton->view->zoom_center = legacy_view.zoom_center;
            milton->view->pan_center = VEC2L(legacy_view.pan_center * -1);
            milton->view->background_color = legacy_view.background_color;
            milton->view->working_layer_id = legacy_view.working_layer_id;
            milton->view->angle = 0.0f;
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
                    Stroke stroke = {};

                    stroke.id = milton->canvas->stroke_id_count++;

                    if ( milton_binary_version < 7 ) {
                        READ(&stroke.brush, sizeof(BrushPreV7), 1, fd);

                        // Previous versions used a magic value for the eraser.
                        v4f k_eraser_color = {23,34,45,56};

                        if (stroke.brush.color == k_eraser_color) {
                            stroke.flags |= StrokeFlag_ERASER;
                        }
                        stroke.brush.hardness = 10.0f;
                    }
                    else if ( milton_binary_version < 8 ) {
                        READ(&stroke.brush, sizeof(BrushPreV8), 1, fd);
                        READ(&stroke.flags, sizeof(stroke.flags), 1, fd);
                        stroke.brush.hardness = 2.0f;
                    }
                    else {
                        if (!read_brushes(&stroke.brush, 1, fd)) {
                            ok = false;
                            goto END;
                        }
                        READ(&stroke.flags, sizeof(stroke.flags), 1, fd);
                    }

                    READ(&stroke.num_points, sizeof(i32), 1, fd);

                    if ( stroke.num_points > STROKE_MAX_POINTS || stroke.num_points <= 0 ) {
                        milton_log("ERROR: File has a stroke with %d points\n",
                                   stroke.num_points);
                        // Older versions have a possible off-by-one bug here.
                        if (stroke.num_points == STROKE_MAX_POINTS)  {
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

                // Set the flags of the working layer to the last stroke of the working layer.
                if (layer->id == saved_working_layer_id) {
                    i64 stroke_count = count(&layer->strokes);
                    if (stroke_count > 0) {
                        milton->working_stroke.flags = layer->strokes[ stroke_count - 1]->flags;
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
            for (int i = 0; i < 2; ++i) {
                READ(&milton->brushes[i], sizeof(BrushPreV7), 1, fd);
            }
            // Sizes
            READ(&milton->brush_sizes, sizeof(i32), 2, fd);
        }
        else if ( milton_binary_version > 5 ) {
            u16 num_brushes = 0;
            READ(&num_brushes, sizeof(u16), 1, fd);
            if ( num_brushes > BrushEnum_COUNT ) {
                milton_log("Error loading file: too many brushes: %d\n", num_brushes);
            }
            if ( milton_binary_version < 7 ) {
                for (int i = 0; i < num_brushes; ++i) {
                    milton->brushes[i] = default_brush();
                    READ(milton->brushes + i, sizeof(BrushPreV7), 1, fd);
                }
            }
            else if (milton_binary_version < 8) {
                for (int i = 0; i < num_brushes; ++i) {
                    milton->brushes[i] = default_brush();
                    READ(milton->brushes + i, sizeof(BrushPreV8), 1, fd);
                }
            }
            else {
                if (!read_brushes(milton->brushes, num_brushes, fd)) {
                    ok = false;
                    goto END;
                }

            }

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

static bool
write_data(void* address, size_t size, size_t count, FILE* fd)
{
    bool ok = true;
    size_t written = fwrite(address, size, count, fd);
    if ( written != count ) {
        ok = false;
    }
    else {
        g_bytes_written += written * size;
    }
    return ok;
}

void
begin_data_tracking()
{
    g_bytes_written = 0;
}

u64
end_data_tracking()
{
    return g_bytes_written;
}

u64
milton_save(Milton* milton)
{
    begin_data_tracking();
    // Declaring variables here to silence compiler warnings about GOTO jumping declarations.
    i32 history_count = 0;
    u32 milton_binary_version = 0;
    milton->flags |= MiltonStateFlags_LAST_SAVE_FAILED;  // Assume failure. Remove flag on success.

    int pid = (int)getpid();
    PATH_CHAR tmp_fname[MAX_PATH] = {};
    PATH_SNPRINTF(tmp_fname, MAX_PATH, TO_PATH_STR("%s.mlt_tmp_%d"), milton->persist->mlt_file_path, pid);

    FILE* fd = platform_fopen(tmp_fname, TO_PATH_STR("wb"));

    b32 could_write_milton_state = false;

    if ( fd ) {
        u32 milton_magic = MILTON_MAGIC_NUMBER;

        if ( write_data(&milton_magic, sizeof(u32), 1, fd) ) {
            milton_binary_version = milton->persist->mlt_binary_version;
            i32 num_layers = layer::number_of_layers(milton->canvas->root_layer);

            mlt_assert(sizeof(CanvasView) == milton->view->size);

            if ( write_data(&milton_binary_version, sizeof(u32), 1, fd) &&
                 write_data(milton->view, sizeof(CanvasView), 1, fd) &&
                 write_data(&num_layers, sizeof(i32), 1, fd) &&
                 write_data(&milton->canvas->layer_guid, sizeof(i32), 1, fd) ) {

                //
                // Layer contents
                //

                bool could_write_layer_contents = true;

                for ( Layer* layer = milton->canvas->root_layer;
                      could_write_layer_contents && layer;
                      layer=layer->next  ) {
                    if ( layer->strokes.count > INT_MAX ) {
                        milton_die_gracefully("FATAL. Number of strokes in layer greater than can be stored in file format. ");
                    }
                    i32 num_strokes = (i32)layer->strokes.count;
                    char* name = layer->name;
                    i32 len = (i32)(strlen(name) + 1);

                    bool could_write_strokes = true;
                    bool could_write_effects = true;

                    if ( write_data(&len, sizeof(i32), 1, fd) &&
                         write_data(name, sizeof(char), (size_t)len, fd) &&
                         write_data(&layer->id, sizeof(i32), 1, fd) &&
                         write_data(&layer->flags, sizeof(layer->flags), 1, fd) &&
                         write_data(&num_strokes, sizeof(i32), 1, fd) ) {
                        for ( i32 stroke_i = 0;
                              could_write_strokes && stroke_i < num_strokes;
                              ++stroke_i ) {
                            Stroke* stroke = get(&layer->strokes, stroke_i);
                            mlt_assert(stroke->num_points > 0);
                            if ( stroke->num_points > 0 && stroke->num_points <= STROKE_MAX_POINTS ) {
                                i32 size_of_brush = sizeof(Brush);
                                if ( !write_data(&size_of_brush, sizeof(i32), 1, fd ) ||
                                     !write_data(&stroke->brush, sizeof(Brush), 1, fd) ||
                                     !write_data(&stroke->flags, sizeof(stroke->flags), 1, fd) ||
                                     !write_data(&stroke->num_points, sizeof(i32), 1, fd) ||
                                     !write_data(stroke->points, sizeof(v2l), (size_t)stroke->num_points, fd) ||
                                     !write_data(stroke->pressures, sizeof(f32), (size_t)stroke->num_points, fd) ||
                                     !write_data(&stroke->layer_id, sizeof(i32), 1, fd) ) {
                                    could_write_strokes = false;
                                    break;
                                }
                            } else {
                                milton_log("WARNING: Trying to write a stroke of size %d\n", stroke->num_points);
                            }
                        }
                    } else {
                        could_write_strokes = false;
                    }

                    if ( !could_write_strokes ) {
                        could_write_effects = false;
                    }
                    else {
                        i64 num_effects = 0;
                        for ( LayerEffect* e = layer->effects; e != NULL; e = e->next ) {
                            ++num_effects;
                        }
                        if ( write_data(&num_effects, sizeof(num_effects), 1, fd) ) {
                            for ( LayerEffect* e = layer->effects; e != NULL; e = e->next ) {
                                if ( write_data(&e->type, sizeof(e->type), 1, fd) &&
                                     write_data(&e->enabled, sizeof(e->enabled), 1, fd) ) {
                                    switch (e->type) {
                                        case LayerEffectType_BLUR: {
                                            if ( !write_data(&e->blur.original_scale, sizeof(e->blur.original_scale), 1, fd) ||
                                                 !write_data(&e->blur.kernel_size, sizeof(e->blur.kernel_size), 1, fd) ) {
                                                could_write_effects = false;
                                            }
                                        } break;
                                    }
                                }
                                else {
                                    could_write_effects = false;
                                }
                            }
                        }
                    }
                    if (!could_write_strokes || !could_write_effects) {
                        could_write_layer_contents = false;
                    }
                }

                if ( could_write_layer_contents ) {
                    b32 could_write_picker = true;
                    if ( milton_binary_version >= 5 ) {
                        v3f rgb = gui_get_picker_rgb(milton->gui);
                        could_write_picker = write_data(&rgb, sizeof(rgb), 1, fd);
                    }
                    else {
                        could_write_picker = write_data(&milton->gui->picker.data, sizeof(PickerData), 1, fd);
                    }

                    //
                    // Buttons
                    //
                    b32 could_write_buttons = true;

                    if ( could_write_picker ) {
                        i32 button_count = 0;
                        MiltonGui* gui = milton->gui;
                        // Count buttons
                        for (ColorButton* b = gui->picker.color_buttons; b!= NULL; b = b->next, button_count++) { }
                        // Write
                        could_write_buttons = write_data(&button_count, sizeof(i32), 1, fd);
                        if ( could_write_buttons ) {
                            for ( ColorButton* b = gui->picker.color_buttons;
                                  could_write_buttons && b!= NULL;
                                  b = b->next ) {
                                could_write_buttons = write_data(&b->rgba, sizeof(v4f), 1, fd);
                            }
                        }
                    }
                    else {
                        could_write_buttons = false;
                    }

                    if ( could_write_buttons ) {

                        //
                        // Brush
                        //
                        b32 could_write_brushes = true;

                        i32 size_of_brush = sizeof(Brush);

                        u16 num_brushes = 3;  // Brush, eraser, primitive.
                        if ( !write_data(&num_brushes, sizeof(num_brushes), 1, fd) ||
                             !write_data(&size_of_brush, sizeof(i32), 1, fd) ||
                             !write_data(&milton->brushes, sizeof(Brush), num_brushes, fd) ||
                             !write_data(&milton->brush_sizes, sizeof(i32), num_brushes, fd) ) {
                            could_write_brushes = false;
                        }

                        if ( could_write_brushes ) {
                            history_count = (i32)milton->canvas->history.count;
                            if ( milton->canvas->history.count > INT_MAX ) {
                                history_count = 0;
                            }

                            //
                            // Undo history
                            //

                            if ( write_data(&history_count, sizeof(history_count), 1, fd) &&
                                 write_data(milton->canvas->history.data, sizeof(*milton->canvas->history.data), (size_t)history_count, fd) ) {

                                //
                                // Layer alpha
                                //
                                b32 could_write_layer_alpha = true;

                                if ( milton_binary_version >= 3 ) {
                                    Layer* l = milton->canvas->root_layer;
                                    for ( i64 i = 0;
                                          could_write_layer_alpha && i < num_layers;
                                          ++i ) {
                                        mlt_assert(l);
                                        if ( !write_data(&l->alpha, sizeof(l->alpha), 1, fd) ) {
                                            could_write_layer_alpha = false;
                                        }
                                        l = l->next;
                                    }
                                }

                                //
                                // Done.
                                //
                                if ( could_write_layer_alpha ) {
                                    could_write_milton_state = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        int file_error = ferror(fd);
        if ( file_error == 0 ) {
            int close_ret = fclose(fd);
            if ( close_ret == 0 ) {
                if ( !could_write_milton_state ) {
                    platform_dialog("Milton failed to write to the file!", "Save error.");
                }
                else {
                    if ( platform_move_file(tmp_fname, milton->persist->mlt_file_path) ) {
                        //  \o/
                        milton_save_postlude(milton);
                    }
                    else {
                        milton_log("Could not move file. Moving on. Avoiding this save.\n");
                        milton->flags |= MiltonStateFlags_MOVE_FILE_FAILED;
                    }
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
    }
    u64 bytes_written = end_data_tracking();
    return bytes_written;
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
platform_settings_load(PlatformSettings* prefs)
{
    b32 loaded = false;
    PATH_CHAR fname[MAX_PATH] = TO_PATH_STR("platform_settings.bin");
    platform_fname_at_config(fname, MAX_PATH);

    milton_log("Prefs file: %s\n", fname);

    *prefs = {};

    if ( FILE* fd = platform_fopen(fname, TO_PATH_STR("rb")) ) {
        if ( !ferror(fd) ) {
            u16 prefs_size = 0;
            fread(&prefs_size, sizeof(u16), 1, fd);

            if (prefs_size <= sizeof(*prefs)) {
                loaded = fread(prefs, prefs_size, 1, fd);
            }
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
platform_settings_save(PlatformSettings* prefs)
{
    PATH_CHAR fname[MAX_PATH] = TO_PATH_STR("platform_settings.bin");
    platform_fname_at_config(fname, MAX_PATH);
    FILE* fd = platform_fopen(fname, TO_PATH_STR("wb"));
    if ( fd && !ferror(fd) ) {
        u16 prefs_size = sizeof(PlatformSettings);
        fwrite(&prefs_size, sizeof(u16), 1, fd);
        fwrite(prefs, sizeof(*prefs), 1, fd);
        fclose(fd);
    }
    else {
        milton_log("Could not open file for writing prefs :(\n");
    }
}

b32
milton_settings_load(MiltonSettings* settings)
{
    PATH_CHAR settings_fname[MAX_PATH] = TO_PATH_STR("user_settings.bin"); {
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

    return ok;
}

void milton_settings_save(MiltonSettings* settings)
{
    PATH_CHAR settings_fname[MAX_PATH] = TO_PATH_STR("user_settings.bin"); {
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
