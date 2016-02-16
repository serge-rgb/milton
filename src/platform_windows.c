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
#include "memory.h"


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

static char* win32_filter_strings =
    "PNG file\0" "*.png\0"
    "JPEG file\0" "*.jpg\0"
    "\0";

char* platform_save_dialog()
{
    static char save_filename[MAX_PATH];

    save_filename[0] = '\0';

    OPENFILENAMEA ofn = {0};

    ofn.lStructSize = sizeof(OPENFILENAME);
    //ofn.hInstance;
    ofn.lpstrFilter = (LPCSTR)win32_filter_strings;
    ofn.lpstrFile = save_filename;
    ofn.nMaxFile = MAX_PATH;
    /* ofn.lpstrInitialDir; */
    /* ofn.lpstrTitle; */
    ofn.Flags = OFN_HIDEREADONLY;
    /* ofn.nFileOffset; */
    /* ofn.nFileExtension; */
    ofn.lpstrDefExt = "jpg";
    /* ofn.lCustData; */
    /* ofn.lpfnHook; */
    /* ofn.lpTemplateName; */

    b32 ok = GetSaveFileNameA(&ofn);

    char* result = NULL;

    if ( ok ) {
        result = save_filename;
    }
    return result;
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

void platform_move_file(char* src, char* dest)
{
    b32 ok = MoveFileExA(src, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
    if ( !ok ) {
        platform_dialog("Could not move file", "Error");
    }
}

void platform_fname_at_config(char* fname, i32 len)
{
    char* base = SDL_GetPrefPath(NULL, "milton");
    char* tmp = mlt_calloc(1, len);
    strncpy(tmp, fname, len);
    fname[0] = '\0';
    strncat(fname, base, len);
    size_t pathlen = strlen(fname);
    strncat(fname + pathlen, tmp, len-pathlen);
    mlt_free(tmp) ;
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
