// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.


#include "persist.h"

#include "common.h"

#include <stb_image_write.h>

#include "gui.h"
#include "milton.h"
#include "platform.h"
#include "tiny_jpeg.h"

#define MILTON_MAGIC_NUMBER 0X11DECAF3

static u32 word_swap_memory_order(u32 word)
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
    FILE* fd = fopen("MiltonPersist.mlt", "rb");
    b32 ok = true;  // fread check

    if ( fd ) {
        u32 milton_magic = (u32)-1;
        if ( ok ) { ok = fread_checked(&milton_magic, sizeof(u32), 1, fd); }
        u32 milton_binary_version = (u32)-1;
        if ( ok ) { ok = fread_checked(&milton_binary_version, sizeof(u32), 1, fd); }

        if (milton_binary_version != 1) {
            ok = false;
        }

        if ( ok ) { ok = fread_checked(milton_state->view, sizeof(CanvasView), 1, fd); }

        milton_magic = word_swap_memory_order(milton_magic);

        if ( milton_magic != MILTON_MAGIC_NUMBER ) {
            assert (!"Magic number not found");
            ok = false;
        }

        if ( ok ) {
            i32 num_strokes = -1;
            if ( ok ) { ok = fread_checked(&num_strokes, sizeof(i32), 1, fd); }

            assert (num_strokes >= 0);

            for ( i32 stroke_i = 0; ok && stroke_i < num_strokes; ++stroke_i ) {
                sb_push(milton_state->strokes, (Stroke){0});
                Stroke* stroke = &sb_peek(milton_state->strokes);
                if ( ok ) { ok = fread_checked(&stroke->brush, sizeof(Brush), 1, fd); }
                if ( ok ) { ok = fread_checked(&stroke->num_points, sizeof(i32), 1, fd); }
                if ( stroke->num_points >= STROKE_MAX_POINTS || stroke->num_points <= 0 ) {
                    milton_log("WTF: File has a stroke with %d points\n", stroke->num_points);
                    ok = false;
                    sb_reset(milton_state->strokes);
                    // Corrupt file. Avoid
                    break;
                }
                if ( ok ) {
                    stroke->points = (v2i*)mlt_calloc((size_t)stroke->num_points, sizeof(v2i));
                    ok = fread_checked_nocopy(stroke->points, sizeof(v2i), (size_t)stroke->num_points, fd);
                    if ( !ok ) mlt_free(stroke->pressures);
                }
                if ( ok ) {
                    stroke->pressures = (f32*)mlt_calloc((size_t)stroke->num_points, sizeof(f32));
                    ok = fread_checked_nocopy(stroke->pressures, sizeof(f32), (size_t)stroke->num_points, fd);
                    if ( !ok ) mlt_free (stroke->points);
                }
            }

            if ( ok ) { ok = fread_checked(&milton_state->gui->picker.info, sizeof(PickerData), 1, fd); }
        }
        fclose(fd);
    }
    if ( !ok ) {
        platform_dialog("Tried to load a corrupted Milton file", "Error");
    }
}

void milton_save(MiltonState* milton_state)
{
    size_t num_strokes = sb_count(milton_state->strokes);
    Stroke* strokes = milton_state->strokes;

    int pid = (int)getpid();
    char tmp_fname[MAX_PATH] = {0};
    snprintf(tmp_fname, MAX_PATH, "milton_tmp.%d.mlt", pid);

    platform_fname_at_exe(tmp_fname, MAX_PATH);

    FILE* fd = fopen(tmp_fname, "wb");

    b32 ok = true;

    if ( fd ) {
        u32 milton_magic = word_swap_memory_order(MILTON_MAGIC_NUMBER);

        fwrite(&milton_magic, sizeof(u32), 1, fd);

        u32 milton_binary_version = 1;

        if ( ok && !fwrite_checked(&milton_binary_version, sizeof(u32), 1, fd)) { ok = false; }
        if ( ok && !fwrite_checked(milton_state->view, sizeof(CanvasView), 1, fd)) { ok = false; }
        if ( ok && !fwrite_checked(&num_strokes, sizeof(i32), 1, fd)) { ok = false; }
        if ( ok ) {
            for ( size_t stroke_i = 0; ok && stroke_i < num_strokes; ++stroke_i ) {
                Stroke* stroke = &strokes[stroke_i];
                assert(stroke->num_points > 0);
                if ( ok ) { ok = fwrite_checked(&stroke->brush, sizeof(Brush), 1, fd); }
                if ( ok ) { ok = fwrite_checked(&stroke->num_points, sizeof(i32), 1, fd); }
                if ( ok ) { ok = fwrite_checked(stroke->points, sizeof(v2i), (size_t)stroke->num_points, fd); }
                if ( ok ) { ok = fwrite_checked(stroke->pressures, sizeof(f32), (size_t)stroke->num_points, fd); }
                if ( !ok ) {
                    break;
                }
            }
        } else {
            ok = false;
        }

        if ( ok ) { ok = fwrite_checked(&milton_state->gui->picker.info, sizeof(PickerData), 1, fd); }

        fclose(fd);

        if ( ok ) {
            platform_move_file(tmp_fname, "MiltonPersist.mlt");
        } else {
            milton_log("[DEBUG]: Saving to temp file %s failed. \n", tmp_fname);
        }
    } else {
        assert (!"Could not create file");
        return;
    }

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
