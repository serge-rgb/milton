// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

extern "C"
{

    // -------------------------------  SHlObj.h
#define CSIDL_DESKTOP                   0x0000        // <desktop>
#define CSIDL_INTERNET                  0x0001        // Internet Explorer (icon on desktop)
#define CSIDL_PROGRAMS                  0x0002        // Start Menu\Programs
#define CSIDL_CONTROLS                  0x0003        // My Computer\Control Panel
#define CSIDL_PRINTERS                  0x0004        // My Computer\Printers
#define CSIDL_PERSONAL                  0x0005        // My Documents
#define CSIDL_FAVORITES                 0x0006        // <user name>\Favorites
#define CSIDL_STARTUP                   0x0007        // Start Menu\Programs\Startup
#define CSIDL_RECENT                    0x0008        // <user name>\Recent
#define CSIDL_SENDTO                    0x0009        // <user name>\SendTo
#define CSIDL_BITBUCKET                 0x000a        // <desktop>\Recycle Bin
#define CSIDL_STARTMENU                 0x000b        // <user name>\Start Menu
#define CSIDL_MYDOCUMENTS               CSIDL_PERSONAL //  Personal was just a silly name for My Documents
#define CSIDL_MYMUSIC                   0x000d        // "My Music" folder
#define CSIDL_MYVIDEO                   0x000e        // "My Videos" folder
#define CSIDL_DESKTOPDIRECTORY          0x0010        // <user name>\Desktop
#define CSIDL_DRIVES                    0x0011        // My Computer
#define CSIDL_NETWORK                   0x0012        // Network Neighborhood (My Network Places)
#define CSIDL_NETHOOD                   0x0013        // <user name>\nethood
#define CSIDL_FONTS                     0x0014        // windows\fonts
#define CSIDL_TEMPLATES                 0x0015
#define CSIDL_COMMON_STARTMENU          0x0016        // All Users\Start Menu
#define CSIDL_COMMON_PROGRAMS           0X0017        // All Users\Start Menu\Programs
#define CSIDL_COMMON_STARTUP            0x0018        // All Users\Startup
#define CSIDL_COMMON_DESKTOPDIRECTORY   0x0019        // All Users\Desktop
#define CSIDL_APPDATA                   0x001a        // <user name>\Application Data
#define CSIDL_PRINTHOOD                 0x001b        // <user name>\PrintHood
    // ....
    //

#ifndef _SHFOLDER_H_
#define CSIDL_FLAG_CREATE               0x8000        // combine with CSIDL_ value to force folder creation in SHGetFolderPath()
#endif // _SHFOLDER_H_

#define CSIDL_FLAG_DONT_VERIFY          0x4000        // combine with CSIDL_ value to return an unverified folder path
#define CSIDL_FLAG_DONT_UNEXPAND        0x2000        // combine with CSIDL_ value to avoid unexpanding environment variables
#if (NTDDI_VERSION >= NTDDI_WINXP)
#define CSIDL_FLAG_NO_ALIAS             0x1000        // combine with CSIDL_ value to insure non-alias versions of the pidl
#define CSIDL_FLAG_PER_USER_INIT        0x0800        // combine with CSIDL_ value to indicate per-user init (eg. upgrade)
#endif  // NTDDI_WINXP
#define CSIDL_FLAG_MASK                 0xFF00        // mask for all possible flag values

HRESULT WINAPI SHGetFolderPathW(__reserved HWND hwnd, __in int csidl, __in_opt HANDLE hToken, __in DWORD dwFlags, __out_ecount(MAX_PATH) LPWSTR pszPath);


    //*
    // -------------------------------
    //




// The returns value mean different things, but other than that, we're ok
#ifdef _MSC_VER
#define snprintf sprintf_s
#endif

#define PATH_STRLEN wcslen
#define PATH_TOLOWER towlower
#define PATH_STRCMP wcscmp
#define PATH_STRNCPY wcsncpy
#define PATH_STRCPY wcscpy
#define PATH_STRCAT wcscat
//#define PATH_STRNCAT wcsncat
#define PATH_SNPRINTF _snwprintf
#define PATH_FPUTS  fputws

#if MILTON_DEBUG
    #define BREAKHERE __debugbreak()
#endif

static FILE* g_win32_logfile;
static i32 g_cursor_count = 0;

int
_path_snprintf(PATH_CHAR* buffer, size_t count, const PATH_CHAR* format, ...)
{
    va_list args;
    mlt_assert (format != NULL);
    va_start(args, format);

#pragma warning(push)
#pragma warning(disable:4774)
    int result = _snwprintf(buffer,
                            count,
                            format,
                            args);
#pragma warning(pop)
    return result;

}

FILE*
platform_fopen(const PATH_CHAR* fname, const PATH_CHAR* mode)
{
    FILE* fd = _wfopen(fname, mode);
    return fd;
}

void*
platform_allocate(size_t size)
{
    void* result = VirtualAlloc(NULL,
                                (size),
                                MEM_COMMIT | MEM_RESERVE,
                                PAGE_READWRITE);
    return result;
}

void
platform_deallocate_internal(void* pointer)
{
    VirtualFree(pointer, 0, MEM_RELEASE);
}

void
win32_debug_output(char* str)
{
    if (g_win32_logfile)
    {
        fputs(str, g_win32_logfile);
    }
}

void
win32_log(char *format, ...)
{
    char message[ 4096 ];

    int num_bytes_written = 0;

    va_list args;

    mlt_assert (format != NULL);

    va_start(args, format);

    num_bytes_written = _vsnprintf(message, sizeof( message ) - 1, format, args);

    if ( num_bytes_written > 0 ) {
        #if WIN32_DEBUGGER_OUTPUT
        OutputDebugStringA(message);
        #endif

        win32_debug_output(message);
    }

    va_end( args );
}

void
milton_fatal(char* message)
{
    milton_log("*** [FATAL] ***: \n\t");
    milton_log(message);
    exit(EXIT_FAILURE);
}

void
milton_die_gracefully(char* message)
{
    platform_dialog(message, "Fatal Error");
    exit(EXIT_FAILURE);
}

static PATH_CHAR* win32_filter_strings_image =
    L"PNG file\0" L"*.png\0"
    L"JPEG file\0" L"*.jpg\0"
    L"\0";

static PATH_CHAR* win32_filter_strings_milton =
    L"MLT file\0" L"*.mlt\0"
    L"\0";

void
win32_set_OFN_filter(OPENFILENAMEW* ofn, FileKind kind)
{
#pragma warning(push)
#pragma warning(disable:4061)
    switch( kind ) {
    case FileKind_IMAGE: {
        ofn->lpstrFilter = (LPCWSTR)win32_filter_strings_image;
        ofn->lpstrDefExt = L"jpg";
    } break;
    case FileKind_MILTON_CANVAS: {
        ofn->lpstrFilter = (LPCWSTR)win32_filter_strings_milton;
        ofn->lpstrDefExt = L"mlt";
    } break;
    default: {
        milton_die_gracefully("Invalid filter in Open File Dialog.");
    } break;
    }
#pragma warning(pop)
}

PATH_CHAR*
platform_save_dialog(FileKind kind)
{
    platform_cursor_show();
    PATH_CHAR* save_filename = (PATH_CHAR*)mlt_calloc(1, MAX_PATH*sizeof(PATH_CHAR), "Strings");

    OPENFILENAMEW ofn = {0};

    ofn.lStructSize = sizeof(OPENFILENAME);
    //ofn.hInstance;
    win32_set_OFN_filter(&ofn, kind);
    ofn.lpstrFile = save_filename;
    ofn.nMaxFile = MAX_PATH;
    /* ofn.lpstrInitialDir; */
    /* ofn.lpstrTitle; */
    //ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    /* ofn.nFileOffset; */
    /* ofn.nFileExtension; */
    //ofn.lpstrDefExt = "jpg";
    /* ofn.lCustData; */
    /* ofn.lpfnHook; */
    /* ofn.lpTemplateName; */

    b32 ok = GetSaveFileNameW(&ofn);

    if ( !ok ) {
        mlt_free(save_filename, "Strings");
        return NULL;
    }
    return save_filename;
}

PATH_CHAR*
platform_open_dialog(FileKind kind)
{
    platform_cursor_show();
    OPENFILENAMEW ofn = {0};

    PATH_CHAR* fname = (PATH_CHAR*)mlt_calloc(MAX_PATH, sizeof(PATH_CHAR), "Strings");

    ofn.lStructSize = sizeof(OPENFILENAME);
    win32_set_OFN_filter(&ofn, kind);
    ofn.lpstrFile = fname;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;

    b32 ok = GetOpenFileNameW(&ofn);
    if ( ok == false ) {
        mlt_free(fname, "Strings");
        milton_log("[ERROR] could not open file! Error is %d\n", CommDlgExtendedError());
        return NULL;
    }
    return fname;
}

b32
platform_dialog_yesno(char* info, char* title)
{
    platform_cursor_show();
    i32 yes = MessageBoxA(NULL, //_In_opt_ HWND    hWnd,
                          (LPCSTR)info, // _In_opt_ LPCTSTR lpText,
                          (LPCSTR)title,// _In_opt_ LPCTSTR lpCaption,
                          MB_YESNO//_In_     UINT    uType
                         );
    return yes == IDYES;
}

void
platform_dialog(char* info, char* title)
{
    platform_cursor_show();
    MessageBoxA( NULL, //_In_opt_ HWND    hWnd,
                 (LPCSTR)info, // _In_opt_ LPCTSTR lpText,
                 (LPCSTR)title,// _In_opt_ LPCTSTR lpCaption,
                 MB_OK//_In_     UINT    uType
               );
}

void
platform_fname_at_exe(PATH_CHAR* fname, size_t len)
{
    PATH_CHAR* tmp = (PATH_CHAR*)mlt_calloc(1, len, "Strings");  // store the fname here
    PATH_STRCPY(tmp, fname);

    DWORD path_len = GetModuleFileNameW(NULL, fname, (DWORD)len);
    if (path_len > len)
    {
        milton_die_gracefully("Milton's install directory has a path that is too long.");
    }

    {  // Remove the exe name
        PATH_CHAR* last_slash = fname;
        for ( PATH_CHAR* iter = fname;
              *iter != '\0';
              ++iter ) {
            if ( *iter == '\\' ) {
                last_slash = iter;
            }
        }
        *(last_slash+1) = '\0';
    }

    PATH_STRCAT(fname, tmp);
    mlt_free(tmp, "Strings");

}

b32
platform_delete_file_at_config(PATH_CHAR* fname, int error_tolerance)
{
    b32 ok = true;
    PATH_CHAR* full = (PATH_CHAR*)mlt_calloc(MAX_PATH, sizeof(*full), "Strings");
    PATH_STRNCPY(full, fname, MAX_PATH);
    platform_fname_at_config(full, MAX_PATH);
    int r = DeleteFileW(full);
    if ( r == 0 ) {
        ok = false;

        int err = (int)GetLastError();
        if ( (error_tolerance&DeleteErrorTolerance_OK_NOT_EXIST) &&
             err == ERROR_FILE_NOT_FOUND ) {
            ok = true;
        }
    }
    mlt_free(full, "Strings");
    return ok;
}

void
win32_print_error(int err)
{
#pragma warning (push,0)
    LPTSTR error_text = NULL;

    FormatMessage(
                  // use system message tables to retrieve error text
                  FORMAT_MESSAGE_FROM_SYSTEM
                  // allocate buffer on local heap for error text
                  |FORMAT_MESSAGE_ALLOCATE_BUFFER
                  // Important! will fail otherwise, since we're not
                  // (and CANNOT) pass insertion parameters
                  |FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
                  err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&error_text,  // output
                  0, // minimum size for output buffer
                  NULL);   // arguments - see note

    if ( error_text ) {
        milton_log(error_text);
        LocalFree(error_text);
    }
    milton_log("- %d\n", err);
#pragma warning (pop)
}

b32
platform_move_file(PATH_CHAR* src, PATH_CHAR* dest)
{
    b32 ok = MoveFileExW(src, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
    //b32 ok = MoveFileExA(src, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        int err = (int)GetLastError();
        win32_print_error(err);

        BOOL could_delete = DeleteFileW(src);
        if ( could_delete == FALSE ) {
            win32_print_error((int)GetLastError());

        }
    }
    return ok;
}

void
platform_fname_at_config(PATH_CHAR* fname, size_t len)
{
    //PATH_CHAR* base = SDL_GetPrefPath("MiltonPaint", "data");
    WCHAR path[MAX_PATH];
    WCHAR* worg = L"MiltonPaint";
    WCHAR* wapp = L"data";
    size_t new_wpath_len = 0;
    BOOL api_result = FALSE;

    if ( !SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path)) ) {
        milton_die_gracefully("Couldn't locate our prefpath");
    }

    new_wpath_len = lstrlenW(worg) + lstrlenW(wapp) + lstrlenW(path) + (size_t)3;

    if ( (new_wpath_len + 1) < MAX_PATH ) {
        lstrcatW(path, L"\\");
        lstrcatW(path, worg);

        api_result = CreateDirectoryW(path, NULL);
        if ( api_result == FALSE ) {
            auto last_error = GetLastError();
            if ( last_error != ERROR_ALREADY_EXISTS ) {
                win32_print_error((int)last_error);
                milton_die_gracefully("Unexpected error when getting file name at config directory.");
            }
        }

        lstrcatW(path, L"\\");
        lstrcatW(path, wapp);

        api_result = CreateDirectoryW(path, NULL);
        if ( api_result == FALSE ) {
            auto last_error = GetLastError();
            if ( last_error != ERROR_ALREADY_EXISTS ) {
                win32_print_error((int)last_error);
                milton_die_gracefully("Unexpected error when getting file name at config directory.");
            }
        }

        lstrcatW(path, L"\\");

        PATH_CHAR* tmp = (PATH_CHAR*)mlt_calloc(1, len*sizeof(PATH_CHAR), "Strings");  // store the fname here
        PATH_STRCPY(tmp, fname);
        fname[0] = '\0';
        wcsncat(fname, path, len);
        wcsncat(fname, tmp, len);
        mlt_free(tmp, "Strings");
    }
}

// Delete old milton_tmp files from previous milton versions.
void
win32_cleanup_appdata()
{
    PATH_CHAR fname[MAX_PATH] = {};
    platform_fname_at_config(fname, MAX_PATH);

    PATH_CHAR* base_end = fname + wcslen(fname);

    wcscat((PATH_CHAR*)fname, L"milton_tmp.*.mlt");
    WIN32_FIND_DATAW find_data = {};

    HANDLE hfind = FindFirstFileW(fname, &find_data);
    if ( hfind != INVALID_HANDLE_VALUE ) {
        b32 can_delete = true;
        while ( can_delete ) {
            PATH_CHAR* fn = find_data.cFileName;
            //milton_log("AppData Cleanup: Deleting %s\n", fn);

            *base_end = '\0';

            wcscat(fname, fn);

            BOOL could_delete = DeleteFileW(fname);
            if ( could_delete == FALSE ) {
                win32_print_error((int)GetLastError());
            }
            can_delete = FindNextFileW(hfind, &find_data);
        }
        FindClose(hfind);
    }
}

void
platform_open_link(char* link)
{
    ShellExecute(NULL, "open", link, NULL, NULL, SW_SHOWNORMAL);
}

WallTime
platform_get_walltime()
{
    WallTime wt = {0};
    {
        SYSTEMTIME ST;
        GetLocalTime(&ST);
        wt.h = ST.wHour;
        wt.m = ST.wMinute;
        wt.s = ST.wSecond;
        wt.ms = ST.wMilliseconds;
    }
    return wt;
}

u64
perf_counter()
{
    u64 time = 0;
    LARGE_INTEGER li = {0};
    // On XP and higher, this function always succeeds.
    QueryPerformanceCounter(&li);

    time = (u64)li.QuadPart;

    return time;
}

float
perf_count_to_sec(u64 counter)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    float sec = (float)counter / (float)freq.QuadPart;
    return sec;
}

void
platform_cursor_hide()
{
    while ( g_cursor_count >= 0 ) {
        ShowCursor(FALSE);
        g_cursor_count--;
    }
}

void
platform_cursor_show()
{
    while ( g_cursor_count < 0 ) {
        ShowCursor(TRUE);
        g_cursor_count++;
    }
}

i32
platform_monitor_refresh_hz()
{
    i32 hz = 60;
    // Try and get the refresh rate from the main monitor.
    DEVMODE dm = {};
    if ( EnumDisplaySettings(/*deviceName*/NULL, ENUM_CURRENT_SETTINGS, &dm) != 0 ) {
        hz = (i32)dm.dmDisplayFrequency;
    }
    return hz;
}


int
CALLBACK WinMain(HINSTANCE hInstance,
                 HINSTANCE hPrevInstance,
                 LPSTR lpCmdLine,
                 int nCmdShow)
{
    win32_cleanup_appdata();
    PATH_CHAR path[MAX_PATH] = TO_PATH_STR("milton.log");
    platform_fname_at_config(path, MAX_PATH);
    g_win32_logfile = platform_fopen(path, TO_PATH_STR("w"));
    milton_main();
}

} // extern "C"
