// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
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
    // #define _GNU_SOURCE //temporarily targeting gcc for program_invocation_name
    #include <sys/stat.h>
    #include <sys/time.h>
    #include <errno.h>
    #include <time.h>
    #include <ctype.h>

    #define BREAKHERE raise(SIGTRAP)

#elif defined(__MACH__)
    #include <sys/mman.h>
    #include <unistd.h> // getpid
    #else
    #error "This is not the Unix you're looking for"
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

#define platform_milton_log unix_log
#define platform_milton_log_args unix_log_args

void unix_log(char* format, ...);
void unix_log_args(char* format, va_list args);

#if defined(__MACH__)
// Include header for our SDL hook.
#include "platform_OSX_SDL_hooks.h"
#endif
