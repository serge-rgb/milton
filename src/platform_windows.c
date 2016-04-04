// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#if defined(__cplusplus)
extern "C" {
#endif

#include "platform.h"

#include "common.h"
#include "memory.h"
#include "milton_configuration.h"
#include "utils.h"


// The returns value mean different things, but other than that, we're ok
#ifdef _MSC_VER
#define snprintf sprintf_s
#endif

#define HEAP_BEGIN_ADDRESS NULL

void* platform_allocate(size_t size)
{
    return VirtualAlloc(HEAP_BEGIN_ADDRESS,
                        (size),
                        MEM_COMMIT | MEM_RESERVE,
                        PAGE_READWRITE);

}

void platform_deallocate_internal(void* pointer)
{
    VirtualFree(pointer, 0, MEM_RELEASE);
}

void win32_log(char *format, ...)
{
    char message[ 1024 ];

    int num_bytes_written = 0;

    va_list args;

    assert ( format );

    va_start( args, format );

    num_bytes_written = _vsnprintf(message, sizeof( message ) - 1, format, args);

    if ( num_bytes_written > 0 ) {
        OutputDebugStringA( message );
    }

    va_end( args );
}

void milton_fatal(char* message)
{
    milton_log("*** [FATAL] ***: \n\t");
    puts(message);
    exit(EXIT_FAILURE);
}

// TODO: Show a message box, and then die
void milton_die_gracefully(char* message)
{
    milton_log("*** [FATAL] ***: \n\t");
    puts(message);
    exit(EXIT_FAILURE);
}

void platform_load_gl_func_pointers()
{
    GLenum glew_err = glewInit();

    if (glew_err != GLEW_OK) {
        milton_log("glewInit failed with error: %s\nExiting.\n", glewGetErrorString(glew_err));
        milton_die_gracefully("glewInit failed");
    }

    if ( !GLEW_VERSION_2_1 ) {
        milton_die_gracefully("OpenGL 2.1 not supported.\n");
    }
}

static char* win32_filter_strings_image =
    "PNG file\0" "*.png\0"
    "JPEG file\0" "*.jpg\0"
    "\0";

static char* win32_filter_strings_milton =
    "MLT file\0" "*.mlt\0"
    "\0";

void win32_set_OFN_filter(OPENFILENAMEA* ofn, FileKind kind)
{
    switch(kind) {
    case FileKind_IMAGE: {
        ofn->lpstrFilter = (LPCSTR)win32_filter_strings_image;
        ofn->lpstrDefExt = "jpg";
    } break;
    case FileKind_MILTON_CANVAS: {
        ofn->lpstrFilter = (LPCSTR)win32_filter_strings_milton;
        ofn->lpstrDefExt = "mlt";
    } break;
    default: {
        INVALID_CODE_PATH;
    }
    }
}

char* platform_save_dialog(FileKind kind)
{
    char* save_filename = mlt_calloc(MAX_PATH, sizeof(char));

    OPENFILENAMEA ofn = {0};

    ofn.lStructSize = sizeof(OPENFILENAME);
    //ofn.hInstance;
    win32_set_OFN_filter(&ofn, kind);
    ofn.lpstrFile = save_filename;
    ofn.nMaxFile = MAX_PATH;
    /* ofn.lpstrInitialDir; */
    /* ofn.lpstrTitle; */
    ofn.Flags = OFN_HIDEREADONLY;
    /* ofn.nFileOffset; */
    /* ofn.nFileExtension; */
    //ofn.lpstrDefExt = "jpg";
    /* ofn.lCustData; */
    /* ofn.lpfnHook; */
    /* ofn.lpTemplateName; */

    b32 ok = GetSaveFileNameA(&ofn);

    char* result = NULL;

    if (!ok) {
        mlt_free(save_filename);
        return NULL;
    }
    return save_filename;
}

char* platform_open_dialog(FileKind kind)
{
    OPENFILENAMEA ofn = {0};

    char* fname = mlt_calloc(MAX_PATH, sizeof(*fname));

    ofn.lStructSize = sizeof(OPENFILENAME);
    win32_set_OFN_filter(&ofn, kind);
    ofn.lpstrFile = fname;
    ofn.nMaxFile = MAX_PATH;

    b32 ok = GetOpenFileNameA(&ofn);
    if (ok == false) {
        free(fname);
        milton_log("[ERROR] could not open file! Error is %d\n", CommDlgExtendedError());
        return NULL;
    }
    return fname;
}

void platform_dialog(char* info, char* title)
{
    MessageBoxA( NULL, //_In_opt_ HWND    hWnd,
                 (LPCSTR)info, // _In_opt_ LPCTSTR lpText,
                 (LPCSTR)title,// _In_opt_ LPCTSTR lpCaption,
                 MB_OK//_In_     UINT    uType
               );
}

void platform_fname_at_exe(char* fname, i32 len)
{
    char* base = SDL_GetBasePath();  // SDL returns utf8 path!
    char* tmp = mlt_calloc(1, len);
    strncpy(tmp, fname, len);
    fname[0] = '\0';
    strncat(fname, base, len);
    size_t pathlen = strlen(fname);
    strncat(fname + pathlen, tmp, len-pathlen);
    mlt_free(tmp) ;
}

void platform_delete_file_at_config(char* fname)
{
    char* full = mlt_calloc(MAX_PATH, sizeof(*full));
    strncpy(full, fname, MAX_PATH);
    platform_fname_at_config(full, MAX_PATH);
    DeleteFileA(full);
    mlt_free(full);
}

void platform_move_file(char* src, char* dest)
{
    b32 ok = MoveFileExA(src, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
    if ( !ok ) {
        platform_dialog("Could not move file", "Error");
    }
}

void platform_fname_at_config(char* fname, i32 len)
{
    char* base = SDL_GetPrefPath("MiltonPaint", "data");
    char* tmp = mlt_calloc(1, len);
    strncpy(tmp, fname, len);
    fname[0] = '\0';
    strncat(fname, base, len);
    size_t pathlen = strlen(fname);
    strncat(fname + pathlen, tmp, len-pathlen);
    mlt_free(tmp) ;
}

static MiltonStartupFlags win32_parse_cmdline()
{
    MiltonStartupFlags startup_flags = { 0 };

    // Debug configuration always records...
#if MILTON_DEBUG
    startup_flags.history_debug = HistoryDebug_RECORD;
#endif

    char* cmdline = GetCommandLineA();

    char** tokens = str_tokenize(cmdline);

    for ( i32 token_i = 0; token_i < sb_count(tokens); ++token_i ) {
        if ( !strcmp(tokens[token_i], "record") ) {
            startup_flags.history_debug = HistoryDebug_RECORD;
        }
        if ( !strcmp(tokens[token_i], "replay") ) {
            startup_flags.history_debug = HistoryDebug_REPLAY;
        }
    }

    str_free(tokens);

    return startup_flags;
}

int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
{
    MiltonStartupFlags flags = win32_parse_cmdline();
    milton_main(flags);
}

#if defined(__cplusplus)
}
#endif
