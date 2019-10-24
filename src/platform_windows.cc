// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "platform.h"

#include "memory.h"

extern "C" {

static FILE* g_win32_logfile;

struct PlatformSpecific
{
    HWND    hwnd;
    WinDpiApi* win_dpi_api;
};

#define LOAD_DLL_PROC(dll, name) name##Proc* name = (name##Proc*)GetProcAddress(dll, #name);


// Stub functions
SET_PROCESS_DPI_AWARENESS_PROC(  SetProcessDpiAwarenessStub  )
{
    return 0;
}
GET_DPI_FOR_MONITOR_PROC( GetDpiForMonitorStub )
{
    *dpiX = 96;
    *dpiY = 96;
    return 0;
}

void
platform_init(PlatformState* platform, SDL_SysWMinfo* sysinfo)
{
    platform->specific = (PlatformSpecific*)platform_allocate(sizeof(PlatformSpecific));
    platform->specific->win_dpi_api = (WinDpiApi*)mlt_calloc(1, sizeof(WinDpiApi), "Setup");
    win_load_dpi_api(platform->specific->win_dpi_api);

    platform->specific->win_dpi_api->SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    mlt_assert(sysinfo->subsystem == SDL_SYSWM_WINDOWS);

    // Handle the case where the window was too big for the screen.
    HWND hwnd = sysinfo->info.win.window;
    // TODO: Fullscreen
    // if (!is_fullscreen) {
    {
        RECT res_rect;
        RECT win_rect;
        HWND dhwnd = GetDesktopWindow();
        GetWindowRect(dhwnd, &res_rect);
        GetClientRect(hwnd, &win_rect);

        platform->specific->hwnd = hwnd;

        i32 snap_threshold = 300;
        if (win_rect.right != platform->width
            || win_rect.bottom != platform->height
            // Also maximize if the size is large enough to "snap"
            || (win_rect.right + snap_threshold >= res_rect.right
                && win_rect.left + snap_threshold >= res_rect.left)
            || win_rect.left < 0
            || win_rect.top < 0) {
            // Our prefs weren't right. Let's maximize.

            SetWindowPos(hwnd, HWND_TOP, 20, 20, win_rect.right - 20, win_rect.bottom - 20, SWP_SHOWWINDOW);
            platform->width = win_rect.right - 20;
            platform->height = win_rect.bottom - 20;
            ShowWindow(hwnd, SW_MAXIMIZE);
        }
    }
    // Load EasyTab
    EasyTabResult easytab_res = EasyTab_Load(platform->specific->hwnd);
    if (easytab_res != EASYTAB_OK) {
        milton_log("EasyTab failed to load. Code %d\n", easytab_res);
    }

}

void
platform_deinit(PlatformState* platform)
{
    EasyTab_Unload();
}

int
platform_titlebar_height(PlatformState* p)
{
    HWND window = p->specific->hwnd;
    TITLEBARINFO ti = {};

    ti.cbSize = sizeof(TITLEBARINFO);

    GetTitleBarInfo(window, &ti);

    int height = ti.rcTitleBar.bottom - ti.rcTitleBar.top;

    return height;
}

void
platform_setup_cursor(Arena* arena, PlatformState* platform)
{
    {  // Load icon (Win32)
        int si = sizeof(HICON);
        HINSTANCE handle = GetModuleHandle(nullptr);
        PATH_CHAR icopath[MAX_PATH] = L"milton_icon.ico";
        platform_fname_at_exe(icopath, MAX_PATH);
        HICON icon = (HICON)LoadImageW(NULL, icopath, IMAGE_ICON, /*W*/0, /*H*/0,
                                       LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
        if ( icon != NULL ) {
            SendMessage(platform->specific->hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        }
    }

    // Setup hardware cursor.

#if MILTON_HARDWARE_BRUSH_CURSOR
    {  // Set brush HW cursor
        milton_log("Setting up hardware cursor.\n");
        size_t w = (size_t)GetSystemMetrics(SM_CXCURSOR);
        size_t h = (size_t)GetSystemMetrics(SM_CYCURSOR);

        size_t arr_sz = (w*h+7) / 8;

        char* andmask = arena_alloc_array(arena, arr_sz, char);
        char* xormask = arena_alloc_array(arena, arr_sz, char);

        i32 counter = 0;
        {
            size_t cx = w/2;
            size_t cy = h/2;
            for ( size_t j = 0; j < h; ++j ) {
                for ( size_t i = 0; i < w; ++i ) {
                    size_t dist = (i-cx)*(i-cx) + (j-cy)*(j-cy);

                    // 32x32 default;
                    i64 girth = 3; // girth of cursor in pixels
                    size_t radius = 8;
                    if ( w == 32 && h == 32 ) {
                        // 32x32
                    }
                    else if ( w == 64 && h == 64 ) {
                        girth *= 2;
                        radius *= 2;
                    }
                    else {
                        milton_log("WARNING: Got an unexpected cursor size of %dx%d. Using 32x32 and hoping for the best.\n", w, h);
                        w = 32;
                        h = 32;
                        cx = 16;
                        cy = 16;
                    }
                    i64 diff        = (i64)(dist - SQUARE(radius));
                    b32 in_white = diff < SQUARE(girth-0.5f) && diff > -SQUARE(girth-0.5f);
                    diff = (i64)(dist - SQUARE(radius+1));
                    b32 in_black = diff < SQUARE(girth) && diff > -SQUARE(girth);

                    size_t idx = j*w + i;

                    size_t ai = idx / 8;
                    size_t bi = idx % 8;

                    // This code block for windows CreateCursor
#if 0
                    if (incircle &&
                        // Cross-hair effect. Only pixels inside half-radius bands get drawn.
                        (i > cx-radius/2 && i < cx+radius/2 || j > cy-radius/2 && j < cy+radius/2))
                    {
                        if (toggle_black)
                        {
                            xormask[ai] |= (1 << (7 - bi));
                        }
                        else
                        {
                            xormask[ai] &= ~(1 << (7 - bi));
                            xormask[ai] &= ~(1 << (7 - bi));
                        }
                        toggle_black = !toggle_black;
                    }
                    else
                    {
                        andmask[ai] |= (1 << (7 - bi));
                    }
#endif
                    // SDL code block
                    if ( in_white ) {
                        // Cross-hair effect. Only pixels inside half-radius bands get drawn.
                        /* (i > cx-radius/2 && i < cx+radius/2 || j > cy-radius/2 && j < cy+radius/2)) */
                        andmask[ai] &= ~(1 << (7 - bi));  // White
                        xormask[ai] |= (1 << (7 - bi));
                    }
                    else if ( in_black ) {
                        xormask[ai] |= (1 << (7 - bi));  // Black
                        andmask[ai] |= (1 << (7 - bi));

                    }
                    else {
                        andmask[ai] &= ~(1 << (7 - bi));     // Transparent
                        xormask[ai] &= ~(1 << (7 - bi));
                    }
                }
            }
        }
        //platform->hcursor = CreateCursor(/*HINSTANCE*/ 0,
        //                                      /*xHotSpot*/(int)(w/2),
        //                                      /*yHotSpot*/(int)(h/2),
        //                                      /* nWidth */(int)w,
        //                                      /* nHeight */(int)h,
        //                                      (VOID*)andmask,
        //                                      (VOID*)xormask);

        platform->cursor_brush = SDL_CreateCursor((Uint8*)andmask,
                                                 (Uint8*)xormask,
                                                 (int)w,
                                                 (int)h,
                                                 /*xHotSpot*/(int)(w/2),
                                                 /*yHotSpot*/(int)(h/2));

    }
    mlt_assert(platform->cursor_brush != NULL);
#endif  // MILTON_HARDWARE_BRUSH_CURSOR
}

EasyTabResult
platform_handle_sysevent(PlatformState* platform, SDL_SysWMEvent* sysevent)
{
    EasyTabResult res = EASYTAB_EVENT_NOT_HANDLED;
    mlt_assert(sysevent->msg->subsystem == SDL_SYSWM_WINDOWS);
    res = EasyTab_HandleEvent(sysevent->msg->msg.win.hwnd,
                              sysevent->msg->msg.win.msg,
                              sysevent->msg->msg.win.lParam,
                              sysevent->msg->msg.win.wParam);
    return res;
}

void
platform_event_tick()
{
}

void*
platform_get_gl_proc(char* name)
{
    void* func = NULL;
    func = (void*)wglGetProcAddress(name);
    if ( func == NULL )  {
        static HMODULE dll_handle = LoadLibraryA("OpenGL32.dll");
        if (dll_handle) {
            func = (void*)GetProcAddress(dll_handle, name);
        }

        if (func) {
            milton_log("Loaded %s from OpenGL32.dll\n", name);
        }
        else {
            static const sz msglen = 128;
            char msg[msglen] = {};
            snprintf(msg, msglen, "Could not load function %s\nYour GPU does not support Milton :(", name);
            milton_log(msg);
            milton_die_gracefully(msg);
        }
    }
    else {
        milton_log("Loaded %s rom WGL\n", name);
    }
    return func;
}

void
win_load_dpi_api(WinDpiApi* api) {
    HMODULE shcore = LoadLibrary("Shcore.dll");
    api->GetDpiForMonitor = GetDpiForMonitorStub;
    api->SetProcessDpiAwareness = SetProcessDpiAwarenessStub;
    if (shcore) {
        LOAD_DLL_PROC(shcore, GetDpiForMonitor);
        LOAD_DLL_PROC(shcore, SetProcessDpiAwareness);

        if (GetDpiForMonitor) {
            api->GetDpiForMonitor = GetDpiForMonitor;
        }
        if (SetProcessDpiAwareness) {
            api->SetProcessDpiAwareness = SetProcessDpiAwareness;
        }
    }
}

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
platform_deallocate_internal(void** pointer)
{
    mlt_assert(*pointer);
    VirtualFree(*pointer, 0, MEM_RELEASE);
    *pointer = NULL;
}

void
win32_debug_output(char* str)
{
    if ( g_win32_logfile ) {
        fputs(str, g_win32_logfile);
        fflush(g_win32_logfile);  // Make sure output is written in case of a crash.
    }
}

float
platform_ui_scale(PlatformState* p)
{
    WinDpiApi* api = p->specific->win_dpi_api;
    HMONITOR hmon = MonitorFromWindow(p->specific->hwnd, MONITOR_DEFAULTTONEAREST);
    UINT dpix = 96;
    UINT dpiy = dpix;
    api->GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy);

    float scale = dpix / 96.0;
    return scale;
}

void    platform_point_to_pixel(PlatformState* ps, v2l* inout)
{

}

void    platform_point_to_pixel_i(PlatformState* ps, v2i* inout)
{

}

void    platform_pixel_to_point(PlatformState* ps, v2l* inout)
{

}

void
win32_log(char *format, ...)
{
    va_list args;
    va_start(args, format);

    win32_log_args(format, args);

    va_end(args);
}

void
win32_log_args(char* format, va_list args)
{
    char message[ 4096 ];

    int num_bytes_written = 0;

    mlt_assert (format != NULL);

    num_bytes_written = _vsnprintf(message, sizeof( message ) - 1, format, args);

    if ( num_bytes_written > 0 ) {
        #if WIN32_DEBUGGER_OUTPUT
        OutputDebugStringA(message);
        #endif

        win32_debug_output(message);
    }
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

    OPENFILENAMEW ofn = {};

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
    OPENFILENAMEW ofn = {};

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

YesNoCancelAnswer
platform_dialog_yesnocancel(char* info, char* title)
{
    platform_cursor_show();
    i32 answer = MessageBoxA(NULL, //_In_opt_ HWND    hWnd,
                             (LPCSTR)info, // _In_opt_ LPCTSTR lpText,
                             (LPCSTR)title,// _In_opt_ LPCTSTR lpCaption,
                             MB_YESNOCANCEL//_In_     UINT    uType
                            );
    if ( answer == IDYES )
        return YesNoCancelAnswer::YES_;
    if ( answer == IDNO )
        return YesNoCancelAnswer::NO_;
    return YesNoCancelAnswer::CANCEL_;
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

static v2i
win32_client_to_screen(HWND hwnd, v2i client)
{
    POINT point = { client.x, client.y };
    int res = MapWindowPoints(
        hwnd,
        HWND_DESKTOP,
        &point,
        1
    );

    if (res == 0) {
        // TODO: Handle error
    }

    v2i screen = { point.x, point.y };
    return screen;
}

static v2i
win32_screen_to_client(HWND hwnd, v2i screen)
{
    POINT point = { screen.x, screen.y };
    int res = MapWindowPoints(
        HWND_DESKTOP,
        hwnd,
        &point,
        1
    );

    if (res == 0) {
        // TODO: Handle error
    }

    v2i client = { point.x, point.y };
    return client;
}

void
platform_open_link(char* link)
{
    ShellExecute(NULL, "open", link, NULL, NULL, SW_SHOWNORMAL);
}

WallTime
platform_get_walltime()
{
    WallTime wt = {};
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
    LARGE_INTEGER li = {};
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
    while ( SDL_ShowCursor(-1) == SDL_ENABLE )  {
        SDL_ShowCursor(SDL_DISABLE);
    }
    while ( ShowCursor(FALSE) >=  0 );
}

void
platform_cursor_show()
{
    while ( SDL_ShowCursor(-1) == SDL_DISABLE )  {
        SDL_ShowCursor(SDL_ENABLE);
    }

    while ( ShowCursor(TRUE) < 0 );
}

void
platform_cursor_set_position(PlatformState* platform, v2i pos)
{
    platform->pointer = pos;

    pos = win32_client_to_screen(platform->specific->hwnd, pos);
    SetCursorPos(pos.x, pos.y);

    // Pending mouse move events will have the cursor close to where it was before we set it.
    SDL_FlushEvent(SDL_MOUSEMOTION);
    SDL_FlushEvent(SDL_SYSWMEVENT);
}

v2i
platform_cursor_get_position(PlatformState* platform)
{
    v2i point = {};
    {
        POINT winPoint = {};
        GetCursorPos(&winPoint);
        point = { (i32)winPoint.x, (i32)winPoint.y };

        point = win32_screen_to_client(platform->specific->hwnd, point);
    }
    return point;
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

void
str_to_path_char(char* str, PATH_CHAR* out, size_t out_sz)
{
    if ( out && str ) {
        size_t num_chars_converted = 0;
        mbstowcs_s(&num_chars_converted,
                   out,
                   out_sz / 4,
                   str,
                   out_sz / sizeof(PATH_CHAR));
    }
}


} // extern "C"

