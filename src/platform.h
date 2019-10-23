// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "common.h"
#include "memory.h"

#include "system_includes.h"
#include "utils.h"
#include "vector.h"

#if defined(__cplusplus)
extern "C" {
#endif


// EasyTab for drawing tablet support

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4668)
#endif

#include "easytab.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

enum LayoutType
{
    LayoutType_QWERTY,
    LayoutType_AZERTY,
    LayoutType_QWERTZ,
    LayoutType_DVORAK,
    LayoutType_COLEMAK,
};

struct SDL_Cursor;

struct PlatformSpecific;

struct PlatformState
{
    i32 width;
    i32 height;

    v2i pointer;

    b32 is_space_down;
    b32 is_pointer_down;
    b32 is_middle_button_down;

    b32 is_panning;
    b32 was_panning;
    b32 waiting_for_pan_input; // Start panning from GUI menu.

    v2l pan_start;
    v2l pan_point;

    b32 should_quit;
    u32 window_id;

    i32 num_pressure_results;
    i32 num_point_results;
    b32 stopped_panning;

    b32 force_next_frame;  // Used for IMGUI, since some operations take 1+ frames.

    // SDL Cursors
    SDL_Cursor* cursor_default;
    SDL_Cursor* cursor_hand;
    SDL_Cursor* cursor_crosshair;
    SDL_Cursor* cursor_sizeall;
    SDL_Cursor* cursor_brush;  // Custom cursor.

    // Current keyboard layout.
    LayoutType keyboard_layout;

    SDL_Window* window;

    PlatformSpecific* specific;

    float ui_scale;
};

typedef enum HistoryDebug
{
    HistoryDebug_NOTHING,

    HistoryDebug_RECORD,
    HistoryDebug_REPLAY,
} HistoryDebug;

typedef struct MiltonStartupFlags
{
    HistoryDebug history_debug;
} MiltonStartupFlags;

typedef struct TabletState_s TabletState;

int milton_main(bool is_fullscreen, char* file_to_open);

void    platform_init(PlatformState* platform, SDL_SysWMinfo* sysinfo);
void    platform_deinit(PlatformState* platform);

void    platform_setup_cursor(Arena* arena, PlatformState* platform);

void    platform_cursor_set_position(PlatformState* platform, v2i pos);
// Get cursor position in client-rect space, whether or not it is within the client rect.
v2i     platform_cursor_get_position(PlatformState* platform);

EasyTabResult platform_handle_sysevent(PlatformState* platform, SDL_SysWMEvent* sysevent);
void          platform_event_tick();

void*   platform_allocate(size_t size);
#define platform_deallocate(pointer) platform_deallocate_internal((void**)&(pointer));
void    platform_deallocate_internal(void** ptr);
float   platform_ui_scale(PlatformState* p);
void    platform_point_to_pixel(PlatformState* ps, v2l* inout);
void    platform_point_to_pixel_i(PlatformState* ps, v2i* inout);
void    platform_pixel_to_point(PlatformState* ps, v2l* inout);

#define milton_log platform_milton_log
#define milton_log_args platform_milton_log_args
void    milton_fatal(char* message);
void    milton_die_gracefully(char* message);

int platform_titlebar_height(PlatformState* p);

void cursor_show();
void cursor_hide();

enum FileKind
{
    FileKind_IMAGE,
    FileKind_MILTON_CANVAS,

    FileKind_COUNT,
};


#if !defined(MAX_PATH) && defined(PATH_MAX)
    #define MAX_PATH PATH_MAX
#endif

// BACKWARDS-COMPATIBILITY NOTE: Should only grow down.
struct PlatformSettings
{
    // Store the window size at the time of quitting.
    i32 width;
    i32 height;

    // Last opened file.
    PATH_CHAR last_mlt_file[MAX_PATH];

    // GUI settings.
    i32 brush_window_left;
    i32 brush_window_top;
    i32 brush_window_width;
    i32 brush_window_height;

    i32 layer_window_left;
    i32 layer_window_top;
    i32 layer_window_width;
    i32 layer_window_height;
};

// Defined in platform_windows.cc
FILE*   platform_fopen(const PATH_CHAR* fname, const PATH_CHAR* mode);

// Returns a 0-terminated string with the full path of the target file.
// If the user cancels the operation it returns NULL.
PATH_CHAR*   platform_open_dialog(FileKind kind);
PATH_CHAR*   platform_save_dialog(FileKind kind);

void    platform_dialog(char* info, char* title);
b32     platform_dialog_yesno(char* info, char* title);

// NOTE: These constants end with an underscore in order to prevent issues on
// macOS where the Objective-C headers define macros `YES` and `NO` as part of
// the `BOOL` type. Removing the underscores and compiling on macOS causes the
// compiler to attempt macro expansion on these names resulting in errors.
enum YesNoCancelAnswer
{
    YES_,
    NO_,
    CANCEL_,
};
YesNoCancelAnswer platform_dialog_yesnocancel(char* info, char* title);

void*   platform_get_gl_proc(char* name);
void    platform_load_gl_func_pointers();

void    platform_fname_at_exe(PATH_CHAR* fname, size_t len);
b32     platform_move_file(PATH_CHAR* src, PATH_CHAR* dest);

void str_to_path_char(char* str, PATH_CHAR* out, size_t out_sz);
// void path_char_to_str(char* str, PATH_CHAR* out, size_t out_sz);

enum DeleteErrorTolerance
{
    DeleteErrorTolerance_NONE         = 1<<0,
    DeleteErrorTolerance_OK_NOT_EXIST = 1<<1,
};
b32     platform_delete_file_at_config(PATH_CHAR* fname, int error_tolerance);
void    platform_fname_at_config(PATH_CHAR* fname, size_t len);

// Does *not* verify link. Do not expose to user facing inputs.
void    platform_open_link(char* link);

WallTime platform_get_walltime();

u64 difference_in_ms(WallTime start, WallTime end);

void    platform_cursor_hide();
void    platform_cursor_show();

i32 platform_monitor_refresh_hz();

// Microsecond (us) resolution timer.
u64 perf_counter();
float perf_count_to_sec(u64 counter);


#if defined(_WIN32)
#include "platform_windows.h"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.h"
#endif


#if defined(__cplusplus)
}
#endif
