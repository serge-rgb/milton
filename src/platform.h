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

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "common.h"
// EasyTab for drawing tablet support
#include "easytab.h"
// ----

typedef struct TabletState_s TabletState;

int milton_main();

void*   platform_allocate(size_t size);
#define platform_deallocate(pointer) platform_deallocate_internal((pointer)); {(pointer) = NULL;}
void    platform_deallocate_internal(void* ptr);
#define milton_log platform_milton_log
void    milton_fatal(char* message);
void    milton_die_gracefully(char* message);



// Returns a 0-terminated string with the full path of the target file. NULL if error.
wchar_t*    platform_save_dialog();
void        platform_dialog(wchar_t* info, wchar_t* title);


// File I/O
typedef struct PlatformFileHandle_s PlatformFileHandle;

int     platform_open_file_write(wchar_t* fname, PlatformFileHandle* out_handle);
b32     platform_write_data(PlatformFileHandle* handle, void* data, int size);
void    platform_close_file(PlatformFileHandle* handle);

size_t  platform_file_handle_size();  // Returns sizeof(PlatformFileHandle), varies by platform.

void    platform_invalidate_file_handle(PlatformFileHandle* handle);  // In case of error.
b32     platform_file_handle_is_valid(PlatformFileHandle* handle);




void    platform_load_gl_func_pointers();


#if defined(_WIN32)
#define platform_milton_log win32_log
void win32_log(char *format, ...);
#elif defined(__linux__) || defined(__MACH__)
#define platform_milton_log printf
#endif

#if defined(__cplusplus)
}
#endif
