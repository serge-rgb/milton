// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "milton_configuration.h"

#define getpid _getpid
#define platform_milton_log win32_log
#define platform_milton_log_args win32_log_args
void win32_log(char *format, ...);
void win32_log_args(char *format, va_list args);

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



// C standard library function defs.
// --------------------------------



void win_load_clib();

// ---------------


// Shcore.dll

typedef enum _MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI  = 0,
    MDT_ANGULAR_DPI    = 1,
    MDT_RAW_DPI        = 2,
    MDT_DEFAULT        = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;


typedef enum _PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE            = 0,
    PROCESS_SYSTEM_DPI_AWARE       = 1,
    PROCESS_PER_MONITOR_DPI_AWARE  = 2
} PROCESS_DPI_AWARENESS;
#define GET_DPI_FOR_MONITOR_PROC(func) \
    HRESULT WINAPI func (_In_  HMONITOR         hmonitor, \
                         _In_  MONITOR_DPI_TYPE dpiType, \
                         _Out_ UINT             *dpiX, \
                         _Out_ UINT             *dpiY \
                        )



#define SET_PROCESS_DPI_AWARENESS_PROC(name) \
        HRESULT WINAPI name(_In_ PROCESS_DPI_AWARENESS value \
                                             )

typedef GET_DPI_FOR_MONITOR_PROC(GetDpiForMonitorProc);
typedef SET_PROCESS_DPI_AWARENESS_PROC (SetProcessDpiAwarenessProc);

struct WinDpiApi {
    SetProcessDpiAwarenessProc*  SetProcessDpiAwareness;
    GetDpiForMonitorProc*        GetDpiForMonitor;
};

void win_load_dpi_api(WinDpiApi* api);
float platform_ui_scale(PlatformState* p);

#define PATH_STRLEN wcslen
#define PATH_TOLOWER towlower
#define PATH_STRCMP wcscmp
#define PATH_STRNCPY wcsncpy
#define PATH_STRCPY wcscpy
#define PATH_STRCAT wcscat
//#define PATH_STRNCAT wcsncat
#define PATH_SNPRINTF _snwprintf
#define PATH_FPUTS  fputws

}
