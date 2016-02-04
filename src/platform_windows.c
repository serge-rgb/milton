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


#if defined(__cplusplus)
extern "C" {
#endif

#include "platform.h"

#include "common.h"


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
        milton_log("glewInit failed with error: %s\nExiting.\n",
                   glewGetErrorString(glew_err));
        exit(EXIT_FAILURE);
    }

    if ( !GLEW_VERSION_2_1 ) {
        milton_die_gracefully("OpenGL 2.1 not supported.\n");
    }
    // Load extensions
}

static wchar_t* win32_filter_strings =
    L"PNG file\0" L"*.png\0"
    L"JPEG file\0" L"*.jpg\0"
    L"\0";

wchar_t* platform_save_dialog()
{
    static wchar_t save_filename[MAX_PATH];

    save_filename[0] = '\0';

    OPENFILENAMEW ofn = {0};

    ofn.lStructSize = sizeof(OPENFILENAME);
    //ofn.hInstance;
    ofn.lpstrFilter = (LPCWSTR)win32_filter_strings;
    ofn.lpstrFile = save_filename;
    ofn.nMaxFile = MAX_PATH;
    /* ofn.lpstrInitialDir; */
    /* ofn.lpstrTitle; */
    ofn.Flags = OFN_HIDEREADONLY;
    /* ofn.nFileOffset; */
    /* ofn.nFileExtension; */
    ofn.lpstrDefExt = L"jpg";
    /* ofn.lCustData; */
    /* ofn.lpfnHook; */
    /* ofn.lpTemplateName; */

    b32 ok = GetSaveFileNameW(&ofn);

    wchar_t* result = NULL;

    if ( ok ) {
        result = save_filename;
    }
    return result;
}

void platform_dialog(wchar_t* info, wchar_t* title)
{
    MessageBoxW( NULL, //_In_opt_ HWND    hWnd,
            (LPCWSTR)info, // _In_opt_ LPCTSTR lpText,
            (LPCWSTR)title,// _In_opt_ LPCTSTR lpCaption,
            MB_OK//_In_     UINT    uType
            );
}

struct PlatformFileHandle_s {
    HANDLE h;
};

b32 platform_open_file_write(wchar_t* fname, PlatformFileHandle* out_handle)
{
    b32 result = false;

    HANDLE handle = CreateFileW(
            (LPCWSTR) fname, //_In_     LPCTSTR               lpFileName,
            GENERIC_WRITE, //_In_     DWORD                 dwDesiredAccess,
            0, //_In_     DWORD                 dwShareMode,
            NULL, //_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            CREATE_ALWAYS, //_In_     DWORD                 dwCreationDisposition,
            FILE_ATTRIBUTE_NORMAL, // _In_     DWORD                 dwFlagsAndAttributes,
            NULL //_In_opt_ HANDLE                hTemplateFile
            );

    if ( handle != INVALID_HANDLE_VALUE ) {
        result = true;
        out_handle->h = handle;
    }
    return result;
}

b32 platform_write_data(PlatformFileHandle* handle, void* data, int size)
{
    b32 result = false;

    int sz_written = 0;
    if ( handle && handle->h != INVALID_HANDLE_VALUE ) {
        BOOL write_result = WriteFile(handle->h, data, (DWORD)size, (LPDWORD)&sz_written, NULL);

#if 0
        LPVOID lpMsgBuf;
        DWORD dw = GetLastError();

        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      dw,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR) &lpMsgBuf,
                      0, NULL );

        // Display the error message and exit the process
        milton_log("%s\n", lpMsgBuf);
#endif

        if ( write_result != FALSE ) {
            if ( sz_written == size ) {
                result = true;
            }
        }
    }
    return result;
}

void platform_close_file(PlatformFileHandle* handle)
{
    if ( handle && handle->h != INVALID_HANDLE_VALUE ) {
        CloseHandle(handle->h);
        *handle = (PlatformFileHandle){ 0 };
    }
}

void platform_invalidate_file_handle(PlatformFileHandle* handle)
{
    if ( handle && handle->h != INVALID_HANDLE_VALUE ) {
        handle->h = INVALID_HANDLE_VALUE;
    }
}

size_t platform_file_handle_size()
{
    return sizeof(PlatformFileHandle);
}

b32 platform_file_handle_is_valid(PlatformFileHandle* handle)
{
    b32 is_valid = false;
    if (handle && handle->h != INVALID_HANDLE_VALUE) {
        is_valid = true;
    }
    return is_valid;
}

int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
{
    milton_main();
}

#if defined(__cplusplus)
}
#endif
