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


typedef struct MiltonStartupFlags
{
    b32 replay;
    b32 record;
} MiltonStartupFlags;

typedef struct TabletState_s TabletState;

int milton_main(MiltonStartupFlags startup_flags);

void*   platform_allocate(size_t size);
#define platform_deallocate(pointer) platform_deallocate_internal((pointer)); {(pointer) = NULL;}
void    platform_deallocate_internal(void* ptr);
#define milton_log platform_milton_log
void    milton_fatal(char* message);
void    milton_die_gracefully(char* message);



// Returns a 0-terminated string with the full path of the target file. NULL if error.
char*   platform_save_dialog();
void    platform_dialog(char* info, char* title);

void    platform_load_gl_func_pointers();

void    platform_fname_at_exe(char* fname, i32 len);
void    platform_move_file(char* src, char* dest);
void    platform_fname_at_config(char* fname, i32 len);


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
