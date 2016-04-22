// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "common.h"
// EasyTab for drawing tablet support

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4668)
#endif

#include "easytab.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(_WIN32)
    // ShellScalingApi.h
typedef enum _PROCESS_DPI_AWARENESS {
  PROCESS_DPI_UNAWARE            = 0,
  PROCESS_SYSTEM_DPI_AWARE       = 1,
  PROCESS_PER_MONITOR_DPI_AWARE  = 2
} PROCESS_DPI_AWARENESS;

HRESULT WINAPI SetProcessDpiAwareness(
  _In_ PROCESS_DPI_AWARENESS value
);
#endif
// ----

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

int milton_main(MiltonStartupFlags startup_flags);

void*   platform_allocate(size_t size);
#define platform_deallocate(pointer) platform_deallocate_internal((pointer)); {(pointer) = NULL;}
void    platform_deallocate_internal(void* ptr);
#define milton_log platform_milton_log
void    milton_fatal(char* message);
void    milton_die_gracefully(char* message);



typedef enum FileKind
{
    FileKind_NOTHING,

    FileKind_IMAGE,
    FileKind_MILTON_CANVAS,
    FileKind_MILTON_CANVAS_NEW,

    FileKind_COUNT,
} FileKind;

// Returns a 0-terminated string with the full path of the target file. NULL if error.
char*   platform_open_dialog(FileKind kind);
char*   platform_save_dialog(FileKind kind);

void    platform_dialog(char* info, char* title);
b32     platform_dialog_yesno(char* info, char* title);

void    platform_load_gl_func_pointers();

void    platform_fname_at_exe(char* fname, i32 len);
b32     platform_move_file(char* src, char* dest);

enum DeleteErrorTolerance
{
    DeleteErrorTolerance_NONE         = 1<<0,
    DeleteErrorTolerance_OK_NOT_EXIST = 1<<1,
};
b32     platform_delete_file_at_config(char* fname, int error_tolerance);
void    platform_fname_at_config(char* fname, i32 len);

void    platform_open_help_link();


#if defined(_WIN32)
#define platform_milton_log win32_log
void win32_log(char *format, ...);
#define getpid _getpid
#elif defined(__linux__) || defined(__MACH__)
#define platform_milton_log printf
#endif

#if defined(__cplusplus)
}
#endif
