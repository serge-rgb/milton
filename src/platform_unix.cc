// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // To get MAP_ANONYMOUS on linux
#endif
#include <gtk/gtk.h>
#define __USE_MISC 1  // MAP_ANONYMOUS and MAP_NORESERVE dont' get defined without this
#include <sys/mman.h>
#undef __USE_MISC
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#if 0
#include <X11/extensions/XIproto.h>
#include <X11/keysym.h>
#endif

#elif defined(__MACH__)
#include <sys/mman.h>
#include <unistd.h> // getpid
#else
#error "This is not the Unix you're looking for"
#endif

#define HEAP_BEGIN_ADDRESS NULL
#if MILTON_DEBUG
#undef HEAP_BEGIN_ADDRESS
#define HEAP_BEGIN_ADDRESS (void*)(1024LL * 1024 * 1024 * 1024)
#endif

#ifndef MAP_ANONYMOUS

#ifndef MAP_ANON
#error "MAP_ANONYMOUS flag not found!"
#else
#define MAP_ANONYMOUS MAP_ANON
#endif

#endif // MAP_ANONYMOUS


#define PATH_STRLEN strlen
#define PATH_TOLOWER tolower
#define PATH_STRCMP strcmp
#define PATH_STRNCPY strncpy
#define PATH_STRCPY strcpy
//#define PATH_STRCAT strcat
#define PATH_STRNCAT strncat
#define PATH_SNPRINTF snprintf
#define platform_milton_log printf

#if defined(__MACH__)
// Include header for our SDL hook.
#include "platform_OSX_SDL_hooks.h"
#endif

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
    u8* ptr = (u8*)mmap(HEAP_BEGIN_ADDRESS, size + sizeof(UnixMemoryHeader),
                        PROT_WRITE | PROT_READ,
                        /*MAP_NORESERVE |*/ MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
    if (ptr)
    {
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

// TODO: haven't checked if platform_cursor_hide or platform_cursor_show work.
void
platform_cursor_hide()
{
    int lvl = SDL_ShowCursor(-1);
    if ( lvl >= 0 )
    {
        mlt_assert ( lvl == 1 );
        int res = SDL_ShowCursor(0);
        if (res < 0)
        {
            INVALID_CODE_PATH;
        }
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

int
main(int argc, char** argv)
{
#ifdef __linux__
    gtk_init(&argc, &argv);
#endif
    milton_main();
}
