// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
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

// Forward decl.
static b32 fread_checked_impl(void* dst, size_t sz, size_t count, FILE* fd, b32 copy);


// Will allocate memory so that if the read fails, we will restore what was
// originally in there.
static b32
fread_checked(void* dst, size_t sz, size_t count, FILE* fd)
{
    return fread_checked_impl(dst, sz, count, fd, false);
}


static b32
fread_checked_impl(void* dst, size_t sz, size_t count, FILE* fd, b32 copy)
{
    b32 ok = false;

    char* raw_data = NULL;
    if ( copy ) {
        raw_data = (char*)mlt_calloc(count, sz, "Persist");
        memcpy(raw_data, dst, count*sz);
    }

    size_t read = fread(dst, sz, count, fd);
    if ( read == count ) {
        if ( !ferror(fd) ) {
            ok = true;
        }
    }

    if ( copy ) {
        if ( !ok ) {
            memcpy(dst, raw_data, count*sz);
        }
        mlt_free(raw_data, "Persist");
    }

    return ok;
}

static b32
fwrite_checked(void* data, size_t sz, size_t count, FILE* fd)
{
    b32 ok = false;

    size_t written = fwrite(data, sz, count, fd);
    if ( written == count ) {
        if ( !ferror(fd) ) {
            ok = true;
        }
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
milton_load(MiltonState* milton_state)
{
    // Reset the canvas.
    milton_reset_canvas(milton_state);

    CanvasState* canvas = milton_state->canvas;

    // Unload gpu data if the strokes have been cooked.
    gpu_free_strokes(milton_state);
    mlt_assert(milton_state->mlt_file_path);
    FILE* fd = platform_fopen(milton_state->mlt_file_path, TO_PATH_STR("rb"));
    b32 ok = true;  // fread check
    b32 handled = false;  // when ok==false but we don't need to prompt a scary message.

    if ( fd ) {
        u32 milton_magic = (u32)-1;
        if (ok) { ok = fread_checked(&milton_magic, sizeof(u32), 1, fd); }
        u32 milton_binary_version = (u32)-1;
        if (ok) { ok = fread_checked(&milton_binary_version, sizeof(u32), 1, fd); }

        if (ok) { milton_state->mlt_binary_version = milton_binary_version; }

        if ( milton_binary_version > MILTON_MINOR_VERSION ) {
            platform_dialog("This file was created with a newer version of Milton.", "Could not open.");

            // Stop loading, but exit without prompting.
            ok = false;
            handled = true;
        }

        auto saved_size = milton_state->view->screen_size;
        if (ok) { ok = fread_checked(milton_state->view, sizeof(CanvasView), 1, fd); }

        // The screen size might hurt us.
        milton_state->view->screen_size = saved_size;

        // The process of loading changes state. working_layer_id changes when creating layers.
        i32 saved_working_layer_id = milton_state->view->working_layer_id;

        if ( milton_magic != MILTON_MAGIC_NUMBER ) {
            platform_dialog("MLT file could not be loaded. Possible endianness mismatch.", "Problem");
            milton_unset_last_canvas_fname();
            ok=false;
        }

        i32 num_layers = 0;
        i32 layer_guid = 0;
        if (ok) { ok = fread_checked(&num_layers, sizeof(i32), 1, fd); }
        if (ok) { ok = fread_checked(&layer_guid, sizeof(i32), 1, fd); }

        for ( int layer_i = 0; ok && layer_i < num_layers; ++layer_i ) {
            i32 len = 0;
            if (ok) { ok = fread_checked(&len, sizeof(i32), 1, fd); }

            if ( len > MAX_LAYER_NAME_LEN ) {
                milton_log("Corrupt file. Layer name is too long.\n");
                ok = false;
            }

            if (ok) { milton_new_layer(milton_state); }

            Layer* layer = milton_state->canvas->working_layer;

            if (ok) { ok = fread_checked(layer->name, sizeof(char), (size_t)len, fd); }

            if (ok) { ok = fread_checked(&layer->id, sizeof(i32), 1, fd); }
            if (ok) { ok = fread_checked(&layer->flags, sizeof(layer->flags), 1, fd); }

            if ( ok ) {
                i32 num_strokes = 0;
                if (ok) { ok = fread_checked(&num_strokes, sizeof(i32), 1, fd); }

                for ( i32 stroke_i = 0; ok && stroke_i < num_strokes; ++stroke_i ) {
                    Stroke stroke = Stroke{};

                    stroke.id = milton_state->canvas->stroke_id_count++;

                    if (ok) { ok = fread_checked(&stroke.brush, sizeof(Brush), 1, fd); }
                    if (ok) { ok = fread_checked(&stroke.num_points, sizeof(i32), 1, fd); }
                    if ( stroke.num_points >= STROKE_MAX_POINTS || stroke.num_points <= 0 ) {
                        if ( stroke.num_points == STROKE_MAX_POINTS ) {
                            // Fix the out of bounds bug.
                            if ( ok ) {
                                stroke.points = arena_alloc_array(&canvas->arena, stroke.num_points, v2i);
                                ok = fread_checked(stroke.points, sizeof(v2i), (size_t)stroke.num_points, fd);
                            }
                            if ( ok ) {
                                stroke.pressures = arena_alloc_array(&canvas->arena, stroke.num_points, f32);
                                ok = fread_checked(stroke.pressures, sizeof(f32), (size_t)stroke.num_points, fd);
                            }

                            if ( ok ) {
                                ok = fread_checked(&stroke.layer_id, sizeof(i32), 1, fd);
                            }
                            pop(&layer->strokes);
                        } else {
                            milton_log("ERROR: File has a stroke with %d points\n", stroke.num_points);
                            ok = false;
                            reset(&milton_state->canvas->root_layer->strokes);
                            // Corrupt file. Avoid
                            break;
                        }
                    } else {
                        if ( ok ) {
                            stroke.points = arena_alloc_array(&canvas->arena, stroke.num_points, v2i);
                            ok = fread_checked(stroke.points, sizeof(v2i), (size_t)stroke.num_points, fd);
                        }
                        if ( ok ) {
                            stroke.pressures = arena_alloc_array(&canvas->arena, stroke.num_points, f32);
                            ok = fread_checked(stroke.pressures, sizeof(f32), (size_t)stroke.num_points, fd);
                        }
                        if ( ok ) {
                            ok = fread_checked(&stroke.layer_id, sizeof(i32), 1, fd);
                        }
                        if ( ok ) {
                            stroke.bounding_rect = bounding_box_for_stroke(&stroke);
                        }

                        layer_push_stroke(layer, stroke);
                    }
                }
            }
        }
        milton_state->view->working_layer_id = saved_working_layer_id;
        if (ok) { ok = fread_checked(&milton_state->gui->picker.data, sizeof(PickerData), 1, fd); }

        // Buttons
        if ( ok ) {
            i32 button_count = 0;
            MiltonGui* gui = milton_state->gui;
            ColorButton* btn = gui->picker.color_buttons;

            if (ok) { ok = fread_checked(&button_count, sizeof(i32), 1, fd); }
            if ( ok ) {
                for ( i32 i = 0;
                      btn!=NULL && i < button_count;
                      ++i, btn=btn->next ) {
                    fread_checked(&btn->rgba, sizeof(v4f), 1, fd);
                }
            }
        }

        // Brush
        if ( milton_binary_version >= 2 ) {
            // PEN, ERASER
            if ( ok ) { ok = fread_checked(&milton_state->brushes, sizeof(Brush), BrushEnum_COUNT, fd); }
            // Sizes
            if ( ok ) { ok = fread_checked(&milton_state->brush_sizes, sizeof(i32), BrushEnum_COUNT, fd); }
        }

        if ( ok ) {
            i32 history_count = 0;
            if ( ok ) { ok = fread_checked(&history_count, sizeof(history_count), 1, fd); }
            if ( ok ) {
                reset(&milton_state->canvas->history);
                reserve(&milton_state->canvas->history, history_count);
            }
            if ( ok ) { ok = fread_checked(milton_state->canvas->history.data, sizeof(*milton_state->canvas->history.data),
                                                (size_t)history_count, fd); }
            if ( ok ) {
                milton_state->canvas->history.count = history_count;
            }
        }

        // MLT 3
        // Layer alpha
        if ( milton_binary_version >= 3 ) {
            Layer* l = milton_state->canvas->root_layer;
            for ( i64 i = 0; ok && i < num_layers; ++i ) {
                mlt_assert(l != NULL);
                fread_checked(&l->alpha, sizeof(l->alpha), 1, fd);
                l = l->next;
            }
        } else {
            for ( Layer* l = milton_state->canvas->root_layer; l != NULL; l = l->next ) {
                l->alpha = 1.0f;
            }
        }

        int err = fclose(fd);
        if ( err != 0 ) {
            ok = false;
        }

        // Finished loading
        if ( !ok ) {
            if ( !handled ) {
                platform_dialog("Tried to load a corrupt Milton file or there was an error reading from disk.", "Error");
            }
            milton_reset_canvas_and_set_default(milton_state);
        } else {
            i32 id = milton_state->view->working_layer_id;
            {  // Use working_layer_id to make working_layer point to the correct thing
                Layer* layer = milton_state->canvas->root_layer;
                while ( layer ) {
                    if ( layer->id == id ) {
                        milton_state->canvas->working_layer = layer;
                        break;
                    }
                    layer = layer->next;
                }
            }
            milton_state->canvas->layer_guid = layer_guid;

            // Update GPU
            milton_set_background_color(milton_state, milton_state->view->background_color);
            gpu_update_picker(milton_state->render_data, &milton_state->gui->picker);
        }
    } else {
        milton_reset_canvas_and_set_default(milton_state);
    }
}

void
milton_save(MiltonState* milton_state)
{
    milton_state->flags |= MiltonStateFlags_LAST_SAVE_FAILED;  // Assume failure. Remove flag on success.

    int pid = (int)getpid();
    PATH_CHAR tmp_fname[MAX_PATH] = {0};
    PATH_SNPRINTF(tmp_fname, MAX_PATH, TO_PATH_STR("milton_tmp.%d.mlt"), pid);

    platform_fname_at_config(tmp_fname, MAX_PATH);

    FILE* fd = platform_fopen(tmp_fname, TO_PATH_STR("wb"));

    b32 ok = true;

    if ( fd ) {
        u32 milton_magic = MILTON_MAGIC_NUMBER;

        fwrite(&milton_magic, sizeof(u32), 1, fd);

        u32 milton_binary_version = milton_state->mlt_binary_version;

        if (ok) { ok = fwrite_checked(&milton_binary_version, sizeof(u32), 1, fd);   }
        if (ok) { ok = fwrite_checked(milton_state->view, sizeof(CanvasView), 1, fd); }

        i32 num_layers = number_of_layers(milton_state->canvas->root_layer);
        if (ok) { ok = fwrite_checked(&num_layers, sizeof(i32), 1, fd); }
        if (ok) { ok = fwrite_checked(&milton_state->canvas->layer_guid, sizeof(i32), 1, fd); }

        i32 test_count = 0;
        for ( Layer* layer = milton_state->canvas->root_layer; layer; layer=layer->next  ) {
            if ( layer->strokes.count > INT_MAX ) {
                milton_die_gracefully("FATAL. Number of strokes in layer greater than can be stored in file format. ");
            }
            i32 num_strokes = (i32)layer->strokes.count;
            char* name = layer->name;
            i32 len = (i32)(strlen(name) + 1);
            if ( ok ) { ok = fwrite_checked(&len, sizeof(i32), 1, fd); }
            if ( ok ) { ok = fwrite_checked(name, sizeof(char), (size_t)len, fd); }
            if ( ok ) { ok = fwrite_checked(&layer->id, sizeof(i32), 1, fd); }
            if ( ok ) { ok = fwrite_checked(&layer->flags, sizeof(layer->flags), 1, fd); }
            if ( ok ) { ok = fwrite_checked(&num_strokes, sizeof(i32), 1, fd); }
            if ( ok ) {
                for ( i32 stroke_i = 0; ok && stroke_i < num_strokes; ++stroke_i ) {
                    Stroke* stroke = get(&layer->strokes, stroke_i);
                    mlt_assert(stroke->num_points > 0);
                    if ( ok ) { ok = fwrite_checked(&stroke->brush, sizeof(Brush), 1, fd); }
                    if ( ok ) { ok = fwrite_checked(&stroke->num_points, sizeof(i32), 1, fd); }
                    if ( ok ) { ok = fwrite_checked(stroke->points, sizeof(v2i), (size_t)stroke->num_points, fd); }
                    if ( ok ) { ok = fwrite_checked(stroke->pressures, sizeof(f32), (size_t)stroke->num_points, fd); }
                    if ( ok ) { ok = fwrite_checked(&stroke->layer_id, sizeof(i32), 1, fd); }
                    if ( !ok ) {
                        break;
                    }
                }
            } else {
                ok = false;
            }
            //milton_log("Saving layer %d with %d strokes\n", test_count+1, num_strokes);
            ++test_count;
        }
        mlt_assert (test_count == num_layers);

        if (ok) { ok = fwrite_checked(&milton_state->gui->picker.data, sizeof(PickerData), 1, fd); }

        // Buttons
        if ( ok ) {
            i32 button_count = 0;
            MiltonGui* gui = milton_state->gui;
            // Count buttons
            for (ColorButton* b = gui->picker.color_buttons; b!= NULL; b = b->next, button_count++) { }
            // Write
            if (ok) { ok = fwrite_checked(&button_count, sizeof(i32), 1, fd); }
            if ( ok ) {
                for ( ColorButton* b = gui->picker.color_buttons;
                      ok && b!= NULL;
                      b = b->next ) {
                    ok = fwrite_checked(&b->rgba, sizeof(v4f), 1, fd);
                }
            }
        }

        // Brush
        if ( milton_binary_version >= 2 ) {
            // PEN, ERASER
            if (ok) { ok = fwrite_checked(&milton_state->brushes, sizeof(Brush), BrushEnum_COUNT, fd); }
            // Sizes
            if (ok) { ok = fwrite_checked(&milton_state->brush_sizes, sizeof(i32), BrushEnum_COUNT, fd); }
        }

        i32 history_count = (i32)milton_state->canvas->history.count;
        if ( milton_state->canvas->history.count > INT_MAX ) {
            history_count = 0;
        }
        if (ok) { ok = fwrite_checked(&history_count, sizeof(history_count), 1, fd); }
        if (ok) { ok = fwrite_checked(milton_state->canvas->history.data, sizeof(*milton_state->canvas->history.data), (size_t)history_count, fd); }

        // MLT 3
        // Layer alpha
        if ( milton_binary_version >= 3 ) {
            Layer* l = milton_state->canvas->root_layer;
            for ( i64 i = 0; ok && i < num_layers; ++i ) {
                mlt_assert(l);
                fwrite_checked(&l->alpha, sizeof(l->alpha), 1, fd);
                l = l->next;
            }
        }

        int file_error = ferror(fd);
        if ( file_error == 0 ) {
            int close_ret = fclose(fd);
            if ( close_ret == 0 ) {
                ok = platform_move_file(tmp_fname, milton_state->mlt_file_path);
                if ( ok ) {
                    //  \o/
                    milton_save_postlude(milton_state);
                }
                else {
                    milton_log("Could not move file. Moving on. Avoiding this save.\n");
                    milton_state->flags |= MiltonStateFlags_MOVE_FILE_FAILED;
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
        // TODO: Fix this on macos
        // milton_die_gracefully("Could not create file for saving! ");
        return;
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

void
milton_prefs_load(PlatformPrefs* prefs)
{
    PATH_CHAR fname[MAX_PATH] = TO_PATH_STR("PREFS.milton_prefs");
    platform_fname_at_config(fname, MAX_PATH);

    FILE* fd = platform_fopen(fname, TO_PATH_STR("rb"));
    if ( fd ) {
        if ( !ferror(fd) ) {
            fread(&prefs->width, sizeof(i32), 1, fd);
            fread(&prefs->height, sizeof(i32), 1, fd);
        }
        else {
            milton_log("Error writing to prefs file...\n");
        }
        fclose(fd);
    }
    else {
        milton_log("Could not open file for writing prefs\n");
    }
}

void
milton_prefs_save(PlatformPrefs* prefs)
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
