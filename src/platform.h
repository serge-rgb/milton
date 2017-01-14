// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "common.h"
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
struct PlatformState
{
    i32 width;
    i32 height;

    v2i pointer;

    b32 is_ctrl_down;
    b32 is_shift_down;
    b32 is_space_down;
    b32 is_pointer_down;
    b32 is_middle_button_down;

    b32 is_panning;
    b32 waiting_for_pan_input; // Start panning from GUI menu.

    b32 was_exporting;
    v2i pan_start;
    v2i pan_point;

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

    // Windows hardware cursor
#if defined(_WIN32)
    HWND    hwnd;
#endif
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

int milton_main();

void*   platform_allocate(size_t size);
#define platform_deallocate(pointer) platform_deallocate_internal((pointer)); {(pointer) = NULL;}
void    platform_deallocate_internal(void* ptr);
#define milton_log platform_milton_log
void    milton_fatal(char* message);
void    milton_die_gracefully(char* message);



void cursor_show();
void cursor_hide();

enum FileKind
{
    FileKind_IMAGE,
    FileKind_MILTON_CANVAS,

    FileKind_COUNT,
};



#define fopen fopen_error

FILE*   fopen_error(const char* fname, const char* mode);
#if !defined(MAX_PATH) && defined(PATH_MAX)
    #define MAX_PATH PATH_MAX
#endif

struct PlatformPrefs
{
    // Store the window size at the time of quitting.
    i32 width;
    i32 height;
    // Last opened file.
    PATH_CHAR last_mlt_file[MAX_PATH];
};

// Defined in platform_windows.cc
FILE*   platform_fopen(const PATH_CHAR* fname, const PATH_CHAR* mode);

// Returns a 0-terminated string with the full path of the target file. NULL if error.
PATH_CHAR*   platform_open_dialog(FileKind kind);
PATH_CHAR*   platform_save_dialog(FileKind kind);

void    platform_dialog(char* info, char* title);
b32     platform_dialog_yesno(char* info, char* title);

void    platform_load_gl_func_pointers();

void    platform_fname_at_exe(PATH_CHAR* fname, size_t len);
b32     platform_move_file(PATH_CHAR* src, PATH_CHAR* dest);

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


void    platform_cursor_hide();
void    platform_cursor_show();

i32 platform_monitor_refresh_hz();

// Microsecond (us) resolution timer.
u64 perf_counter();
float perf_count_to_sec(u64 counter);

#if defined(_WIN32)
#define platform_milton_log win32_log
void win32_log(char *format, ...);
#define getpid _getpid
#elif defined(__linux__) || defined(__MACH__)
#define platform_milton_log printf
#endif


#if defined(_WIN32)
#include "platform_windows.h"
// TODO: Fix linux/mac compilation...
#elif defined(__linux__)
#include "platform_unix.cc"
#include "platform_linux.cc"
#elif defined(__MACH__)
#include "platform_unix.cc"
#include "platform_mac.cc"
#endif


#if defined(__cplusplus)
}
#endif
