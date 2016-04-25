// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "persist.h"

#include "common.h"

#include <stb_image_write.h>

#include "gui.h"
#include "milton.h"
#include "platform.h"
#include "tiny_jpeg.h"

#define MILTON_MAGIC_NUMBER 0X11DECAF3

// Forward decl.
static b32 fread_checked_impl(void* dst, size_t sz, size_t count, FILE* fd, b32 copy);

// Will allocate memory so that if the read fails, we will restore what was
// originally in there.
static b32 fread_checked(void* dst, size_t sz, size_t count, FILE* fd)
{
    return fread_checked_impl(dst, sz, count, fd, true);
}

static b32 fread_checked_nocopy(void* dst, size_t sz, size_t count, FILE* fd)
{
    return fread_checked_impl(dst, sz, count, fd, false);
}

static b32 fread_checked_impl(void* dst, size_t sz, size_t count, FILE* fd, b32 copy)
{
    b32 ok = false;

    char* raw_data = NULL;
    if ( copy ) {
        raw_data = mlt_calloc(count, sz);
        memcpy(raw_data, dst, count*sz);
    }

    size_t read = fread(dst, sz, count, fd);
    if ( read == count ) {
        if ( !ferror(fd) ) {
            ok = true;
        }
    }

    if ( copy  ) {
        if ( !ok ) {
            memcpy(dst, raw_data, count*sz);
        }
        mlt_free(raw_data);
    }

    return ok;
}

static b32 fwrite_checked(void* data, size_t sz, size_t count, FILE* fd)
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


void milton_load(MiltonState* milton_state)
{
    assert(milton_state->mlt_file_path);
    FILE* fd = fopen(milton_state->mlt_file_path, "rb+");
    b32 ok = true;  // fread check

    if ( fd ) {
        u32 milton_magic = (u32)-1;
        if ( ok ) { ok = fread_checked(&milton_magic, sizeof(u32), 1, fd); }
        u32 milton_binary_version = (u32)-1;
        if ( ok ) { ok = fread_checked(&milton_binary_version, sizeof(u32), 1, fd); }

        if ( milton_binary_version != 1 ) {
            ok = false;
        }

        if ( ok ) { ok = fread_checked(milton_state->view, sizeof(CanvasView), 1, fd); }

        // The screen size might hurt us.
        // TODO: Maybe shouldn't save the whole CanvasView?
        milton_state->view->screen_size = (v2i){0};
        // The process of loading changes state. working_layer_id changes when creating layers.
        i32 saved_working_layer_id = milton_state->view->working_layer_id;

        if ( milton_magic != MILTON_MAGIC_NUMBER ) {
            platform_dialog("MLT file could not be loaded. Possible endianness mismatch.", "Problem");
            milton_unset_last_canvas_fname();
            ok=false;
        }

        i32 num_layers = 0;
        i32 layer_guid = 0;
        if ( ok ) { ok = fread_checked(&num_layers, sizeof(i32), 1, fd); }
        if ( ok ) { ok = fread_checked(&layer_guid, sizeof(i32), 1, fd); }

        milton_state->root_layer = NULL;
        milton_state->working_layer = NULL;

        for (int layer_i = 0; layer_i < num_layers; ++layer_i ) {
            i32 len = 0;
            if ( ok ) { ok = fread_checked(&len, sizeof(i32), 1, fd); }

            milton_new_layer(milton_state);
            Layer* layer = milton_state->working_layer;

            if ( ok ) { ok = fread_checked(layer->name, sizeof(char), len, fd); }

            if ( ok ) { ok = fread_checked(&layer->id, sizeof(i32), 1, fd); }
            if ( ok ) { ok = fread_checked(&layer->flags, sizeof(layer->flags), 1, fd); }

            if ( ok ) {
                i32 num_strokes = 0;
                if ( ok ) { ok = fread_checked(&num_strokes, sizeof(i32), 1, fd); }

                for ( i32 stroke_i = 0; ok && stroke_i < num_strokes; ++stroke_i ) {
                    Stroke* stroke = layer_push_stroke(layer, (Stroke){0});

                    if ( ok ) { ok = fread_checked(&stroke->brush, sizeof(Brush), 1, fd); }
                    if ( ok ) { ok = fread_checked(&stroke->num_points, sizeof(i32), 1, fd); }
                    if ( stroke->num_points >= STROKE_MAX_POINTS || stroke->num_points <= 0 ) {
                        milton_log("WTF: File has a stroke with %d points\n", stroke->num_points);
                        ok = false;
                        sb_reset(milton_state->root_layer->strokes);
                        // Corrupt file. Avoid
                        break;
                    }
                    if ( ok ) {
                        stroke->points = (v2i*)mlt_calloc((i32)stroke->num_points, sizeof(v2i));
                        ok = fread_checked_nocopy(stroke->points, sizeof(v2i), (i32)stroke->num_points, fd);
                        if ( !ok ) mlt_free(stroke->pressures);
                    }
                    if ( ok ) {
                        stroke->pressures = (f32*)mlt_calloc((i32)stroke->num_points, sizeof(f32));
                        ok = fread_checked_nocopy(stroke->pressures, sizeof(f32), (i32)stroke->num_points, fd);
                        if ( !ok ) mlt_free (stroke->points);
                    }

                    if ( ok ) {
                        ok = fread_checked(&stroke->layer_id, sizeof(i32), 1, fd);
                    }
                }
            }
        }
        milton_state->view->working_layer_id = saved_working_layer_id;
        if ( ok ) { ok = fread_checked(&milton_state->gui->picker.data, sizeof(PickerData), 1, fd); }

        // Buttons
        if ( ok ) {
            i32 button_count = 0;
            MiltonGui* gui = milton_state->gui;
            ColorButton* btn = &gui->picker.color_buttons;

            if ( ok ) { ok = fread_checked(&button_count, sizeof(i32), 1, fd); }
            if ( ok ) {
                for ( i32 i = 0;
                      btn!=NULL && i < button_count;
                      ++i, btn=btn->next ) {
                    fread_checked(&btn->rgba, sizeof(v4f), 1, fd);
                }
            }
        }

        if ( ok ) {
            i32 history_count = 0;
            if (ok) { ok = fread_checked(&history_count, sizeof(history_count), 1, fd); }
            if (ok) { sb_reserve(milton_state->history, (i32)history_count); }
            if (ok) { ok = fread_checked_nocopy(milton_state->history, sizeof(*milton_state->history), history_count, fd); }
        }
        fclose(fd);
        if ( !ok ) {
            platform_dialog("Tried to load a corrupted Milton file", "Error");
            milton_reset_canvas(milton_state);
        } else {
            i32 id = milton_state->view->working_layer_id;
            {  // Use working_layer_id to make working_layer point to the correct thing
                Layer* layer = milton_state->root_layer;
                while (layer) {
                    if ( layer->id == id ) {
                        milton_state->working_layer = layer;
                        break;
                    }
                    layer = layer->next;
                }
            }
            milton_state->layer_guid = layer_guid;
        }
    } else {
        milton_reset_canvas(milton_state);
    }
}

void milton_save(MiltonState* milton_state)
{
    int pid = (int)getpid();
    char tmp_fname[MAX_PATH] = {0};
    snprintf(tmp_fname, MAX_PATH, "milton_tmp.%d.mlt", pid);

    platform_fname_at_config(tmp_fname, MAX_PATH);

    FILE* fd = fopen(tmp_fname, "wb");

    b32 ok = true;

    if ( fd ) {
        u32 milton_magic = MILTON_MAGIC_NUMBER;

        fwrite(&milton_magic, sizeof(u32), 1, fd);

        u32 milton_binary_version = 1;

        if ( ok ) { ok = fwrite_checked(&milton_binary_version, sizeof(u32), 1, fd);   }
        if ( ok ) { ok = fwrite_checked(milton_state->view, sizeof(CanvasView), 1, fd); }

        i32 num_layers = number_of_layers(milton_state->root_layer);
        if ( ok ) { ok = fwrite_checked(&num_layers, sizeof(i32), 1, fd); }
        if ( ok ) { ok = fwrite_checked(&milton_state->layer_guid, sizeof(i32), 1, fd); }

        i32 TEST = 0;
        for ( Layer* layer = milton_state->root_layer; layer; layer=layer->next ) {
            Stroke* strokes = layer->strokes;
            i32 num_strokes = sb_count(strokes);
            char* name = layer->name;
            i32 len = (i32)(strlen(name) + 1);
            if ( ok ) { ok = fwrite_checked(&len, sizeof(i32), 1, fd); }
            if ( ok ) { ok = fwrite_checked(name, sizeof(char), len, fd); }
            if ( ok ) { ok = fwrite_checked(&layer->id, sizeof(i32), 1, fd); }
            if ( ok ) { ok = fwrite_checked(&layer->flags, sizeof(layer->flags), 1, fd); }
            if ( ok ) { ok = fwrite_checked(&num_strokes, sizeof(i32), 1, fd); }
            if ( ok ) {
                for ( i32 stroke_i = 0; ok && stroke_i < num_strokes; ++stroke_i ) {
                    Stroke* stroke = &strokes[stroke_i];
                    assert(stroke->num_points > 0);
                    if ( ok ) { ok = fwrite_checked(&stroke->brush, sizeof(Brush), 1, fd); }
                    if ( ok ) { ok = fwrite_checked(&stroke->num_points, sizeof(i32), 1, fd); }
                    if ( ok ) { ok = fwrite_checked(stroke->points, sizeof(v2i), (i32)stroke->num_points, fd); }
                    if ( ok ) { ok = fwrite_checked(stroke->pressures, sizeof(f32), (i32)stroke->num_points, fd); }
                    if ( ok ) { ok = fwrite_checked(&stroke->layer_id, sizeof(i32), 1, fd); }
                    if ( !ok ) {
                        break;
                    }
                }
            } else {
                ok = false;
            }
            ++TEST;
        }
        assert (TEST == num_layers);

        if ( ok ) { ok = fwrite_checked(&milton_state->gui->picker.data, sizeof(PickerData), 1, fd); }

        // Buttons
        if ( ok ) {
            i32 button_count = 0;
            MiltonGui* gui = milton_state->gui;
            // Count buttons
            for ( ColorButton* b = &gui->picker.color_buttons; b!= NULL; b = b->next, button_count++ ) { }
            // Write
            if ( ok ) { ok = fwrite_checked(&button_count, sizeof(i32), 1, fd); }
            if ( ok ) {
                for ( ColorButton* b = &gui->picker.color_buttons; ok && b!= NULL; b = b->next ) {
                    ok = fwrite_checked(&b->rgba, sizeof(v4f), 1, fd);
                }
            }
        }


        i32 history_count = sb_count(milton_state->history);
        if ( ok ) { ok = fwrite_checked(&history_count, sizeof(history_count), 1, fd); }
        if ( ok ) { ok = fwrite_checked(milton_state->history, sizeof(*milton_state->history), history_count, fd); }

        fclose(fd);

        if ( ok ) {
            ok = platform_move_file(tmp_fname, milton_state->mlt_file_path);
            if ( !ok ) {
                milton_log("Could not move file. Moving on. Avoiding this save.");
            }
        } else {
            milton_log("[DEBUG]: Saving to temp file %s failed. \n", tmp_fname);
        }
    } else {
        milton_die_gracefully("Could not create file");
        return;
    }
}

void milton_set_last_canvas_fname(char* last_fname)
{
    char* full = mlt_calloc(MAX_PATH, sizeof(char));
    strcpy(full, "last_canvas_fname");
    platform_fname_at_config(full, MAX_PATH);
    FILE* fd = fopen(full, "wb");
    if (fd) {
        u64 len = strlen(last_fname)+1;
        fwrite(&len, sizeof(len), 1, fd);
        fwrite(last_fname, sizeof(char), MAX_PATH, fd);
        fclose(fd);
    }
    mlt_free(full);
}

void milton_unset_last_canvas_fname()
{
    b32 del = platform_delete_file_at_config("last_canvas_fname",DeleteErrorTolerance_OK_NOT_EXIST);
    if (del == false) {
        platform_dialog("The default canvas could not be set to open the next time you run Milton. Please contact the developers.", "Important");
    }
}

char* milton_get_last_canvas_fname()
{
    char* full = mlt_calloc(MAX_PATH, sizeof(char));
    strcpy(full, "last_canvas_fname");
    platform_fname_at_config(full, MAX_PATH);
    FILE* fd = fopen(full, "rb+");
    if (fd) {
        u64 len = 0;
        fread(&len, sizeof(len), 1, fd);
        if (len > MAX_PATH) {
            mlt_free(full);
        } else {
            fread(full, sizeof(char), len, fd);
        }
        fclose(fd);
    } else {
        mlt_free(full);
    }
    return full;
}

// Called by stb_image
static void write_func(void* context, void* data, int size)
{
    FILE* fd = *(FILE**)context;

    if ( fd ) {
        size_t written = fwrite(data, size, 1, fd);
        if ( written != 1 ) {
            fclose(fd);
            *(FILE**)context = NULL;
        }
    }
}

void milton_save_buffer_to_file(char* fname, u8* buffer, i32 w, i32 h)
{
    b32 success = false;

    int len = 0;
    {
        size_t sz = strlen(fname);
        if ( sz > ( (1u << 31) -1 ) ) {
            milton_die_gracefully("A really, really long file name. This shouldn't happen.");
        }
        len = (int)sz;
    }
    char* ext = fname + len;

    // NOTE: This should work with unicode.
    int ext_len = 0;
    b32 found = false;
    {
        int safety = len;
        while ( *--ext != '.' ) {
            if(safety-- == 0) {
                break;
            }
        }
        if (safety > 0) {
            found = true;
            ext_len = len - safety;
            ++ext;
        }
    }

    if ( found ) {
        for ( int i = 0; i < ext_len; ++i ) {
            char c = ext[i];
            ext[i] = (char)tolower(c);
        }

        FILE* fd = NULL;

        fd = fopen(fname, "wb");

        if (  fd ) {
            if ( !strcmp( ext, "png" ) ) {
                stbi_write_png_to_func(write_func, &fd, w, h, 4, buffer, 0);
            } else if ( !strcmp(ext, "jpg") || !strcmp(ext, "jpeg") ) {
                tje_encode_with_func(write_func, &fd, 3, w, h, 4, buffer);
            } else {
                platform_dialog("File extension not handled by Milton\n", "Info");
            }
            // fd might have been set to NULL if write_func failed.
            if ( fd ) {
                if ( ferror(fd) ) {
                    platform_dialog("Unknown error when writing to file :(", "Unknown error");
                } else {
                    platform_dialog("Image exported successfully!", "Success");
                }
                fclose(fd);
            } else {
                platform_dialog("File created, but there was an error writing to it.", "Error");
            }
        } else {
            platform_dialog ( "Could not open file", "Error" );
        }
    } else {
        platform_dialog("File name missing extension!\n", "Error");
    }
}
