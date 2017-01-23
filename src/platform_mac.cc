#define IMPL_MISSING mlt_assert(!"IMPLEMENT")

#include <mach-o/dyld.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "platform.h"
#include "memory.h"

#define MAX_PATH PATH_MAX

// IMPLEMENT ====
float
perf_count_to_sec(u64 counter)
{
    // Input as nanoseconds
    return (float)counter * 1e-9;
}
u64
perf_counter()
{
    // clock_gettime() on macOS is only supported on macOS Sierra and later.
    // For older macOS operating systems, mach_absolute_time() will be need to be used.
    timespec tp;
    int res = clock_gettime(CLOCK_REALTIME, &tp);

    // TODO: Check errno and provide more informations
    if ( res ) {
        milton_log("Something went wrong with clock_gettime\n");
    }

    return tp.tv_nsec;
}

b32
platform_delete_file_at_config(PATH_CHAR* fname, int error_tolerance)
{
    char fname_at_config[MAX_PATH];
    strncpy(fname_at_config, fname, MAX_PATH);
    platform_fname_at_config(fname_at_config, MAX_PATH*sizeof(char));
    int res = remove(fname_at_config);
    b32 result = true;
    if (res != 0)
    {
        result = false;
        // Delete failed for some reason.
        if ((error_tolerance == DeleteErrorTolerance_OK_NOT_EXIST) &&
            (errno == EEXIST || errno == ENOENT))
        {
            result = true;
        }
    }

    return result;
}

void
platform_dialog(char* info, char* title)
{
    IMPL_MISSING;
    return;
}

b32
platform_dialog_yesno(char* info, char* title)
{
    IMPL_MISSING;
    return false;
}

void
platform_fname_at_config(PATH_CHAR* fname, size_t len)
{
    char* string_copy = (char*)mlt_calloc(1, len, "Strings");
    if (string_copy)
    {
        strncpy(string_copy, fname, len);
        snprintf(fname, len,  "~/.milton/%s", string_copy);
        mlt_free(string_copy, "Strings");
    }
}

void
platform_fname_at_exe(PATH_CHAR* fname, size_t len)
{
    u32 bufsize = (u32)len;
    char buffer[MAX_PATH] = {0};
    strncpy(buffer, fname, MAX_PATH);
    _NSGetExecutablePath(fname, &bufsize);
    {  // Remove the executable name
        PATH_CHAR* last_slash = fname;
        for(PATH_CHAR* iter = fname;
            *iter != '\0';
            ++iter)
        {
            if (*iter == '/')
            {
                last_slash = iter;
            }
        }
        *(last_slash+1) = '\0';
    }
    strncat(fname, "/", len);
    strncat(fname, buffer, len);
    return;
}

FILE*
platform_fopen(const PATH_CHAR* fname, const PATH_CHAR* mode)
{
    FILE* fd = fopen_unix(fname, mode);
    return fd;
}

b32
platform_move_file(PATH_CHAR* src, PATH_CHAR* dest)
{
    int res = rename(src, dest);

    return res == 0;
}

PATH_CHAR*
platform_open_dialog(FileKind kind)
{
    IMPL_MISSING;
    return NULL;
}
void
platform_open_link(char* link)
{
    return;
}
PATH_CHAR*
platform_save_dialog(FileKind kind)
{
    IMPL_MISSING;
    return NULL;
}
//  ====

WallTime
platform_get_walltime()
{
    WallTime wt = {0};
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *time;
    time = localtime(&tv.tv_sec);
    wt.h = time->tm_hour;
    wt.m = time->tm_min;
    wt.s = time->tm_sec;
    wt.ms = tv.tv_usec / 1000;
    return wt;
}
