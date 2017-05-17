// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "platform.h"
#include "platform_unix.h"

// Define this function before we substitute fopen in Milton with a macro.
FILE*
fopen_unix(const char* fname, const char* mode)
{
    FILE* fd = fopen(fname, mode);
    return fd;
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

#ifdef __linux__
void        platform_load_gl_func_pointers() {}
#elif __MACH__
void        platform_load_gl_func_pointers() {}
#endif

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
platform_deallocate_internal(void* ptr)
{
    mlt_assert(ptr);
    u8* begin = (u8*)ptr - sizeof(UnixMemoryHeader);
    size_t size = *((size_t*)begin);
    munmap(ptr, size);
}

void
platform_cursor_hide()
{
    int lvl = SDL_ShowCursor(-1);
    if ( lvl >= 0 ) {
        mlt_assert ( lvl == 1 );
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
    int lvl = SDL_ShowCursor(-1);
    if ( lvl < 0 )
    {
        mlt_assert ( lvl == -1 );
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
main(int argc, char** argv)
{
    char* file_to_open = NULL;
    if ( argc == 2 ) {
	file_to_open = argv[1];
    }
    milton_main(file_to_open);
}
