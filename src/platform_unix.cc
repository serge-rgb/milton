// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "platform.h"
#include "platform_unix.h"

static FILE* g_unix_logfile;

void
unix_log_args(char* format, va_list args)
{
    char message [ 4096] = {0};
    int num_bytes_written = 0;
    mlt_assert(format != NULL);

    num_bytes_written = vsnprintf(message, sizeof(message) - 1, format, args);

    if ( num_bytes_written > 0 ) {
        printf("%s", message);
        if (!g_unix_logfile) {
            char fname[MAX_PATH] = TO_PATH_STR("milton.log");
            platform_fname_at_config(fname, MAX_PATH);
            g_unix_logfile = platform_fopen(fname, "wb");
            if (!g_unix_logfile) {
                fprintf(stderr, "ERROR: Can't open log file %s\n", fname);
            }
        }
        if (g_unix_logfile) {
            fwrite(message, 1, num_bytes_written, g_unix_logfile);
        }
    }
}

void
unix_log(char* format, ...)
{
    char message[ 4096 ];

    int num_bytes_written = 0;

    va_list args;

    mlt_assert (format != NULL);

    va_start(args, format);

    unix_log_args(format, args);

    va_end( args );
}

void
milton_fatal(char* message)
{
    milton_log("*** [FATAL] ***: \n\t");
    puts(message);
    exit(EXIT_FAILURE);
}

// TODO: Show a message box, and then die
void
milton_die_gracefully(char* message)
{
    milton_log("*** [FATAL] ***: \n\t");
    milton_log(message);
    exit(EXIT_FAILURE);
}

typedef struct UnixMemoryHeader_s
{
    size_t size;
} UnixMemoryHeader;

void*
platform_allocate(size_t size)
{
    u8* ptr = (u8*)mmap(NULL, size + sizeof(UnixMemoryHeader),
                        PROT_WRITE | PROT_READ,
                        /*MAP_NORESERVE |*/ MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
    if ( ptr ) {
        // NOTE: This should be a footer if we intend on returning aligned data.
        *((UnixMemoryHeader*)ptr) = (UnixMemoryHeader)
        {
            .size = size,
        };
        ptr += sizeof(UnixMemoryHeader);
    }
    return ptr;
}

void
platform_deallocate_internal(void** ptr)
{
    mlt_assert(*ptr);
    u8* begin = (u8*)(*ptr) - sizeof(UnixMemoryHeader);
    size_t size = *((size_t*)begin);
    munmap(*ptr, size);
}

void
platform_cursor_hide()
{
    int shown = SDL_ShowCursor(-1);
    if ( shown ) {
        int res = SDL_ShowCursor(0);
        if ( res < 0 ) {
            INVALID_CODE_PATH;
        }
    }
}

void
str_to_path_char(char* str, PATH_CHAR* out, size_t out_sz)
{
    if ( out && str ) {
        PATH_STRNCPY(out, str, out_sz);
    }
}

void
platform_cursor_show()
{
    int shown = SDL_ShowCursor(-1);
    if ( !shown ) {
        SDL_ShowCursor(1);
    }
}

i32
platform_monitor_refresh_hz()
{
    i32 hz = 60;
    // TODO: Implement this on macOs and Linux.
    return hz;
}

int
platform_titlebar_height(PlatformState* p)
{
    return 20; // TODO: implement on mac and linux
}

int
main(int argc, char** argv)
{
    char* file_to_open = NULL;
    if ( argc == 2 ) {
        file_to_open = argv[1];
    }
    milton_main(false, file_to_open);
}


