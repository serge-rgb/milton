// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include <mach-o/dyld.h>

#define Rect MacRect
// #define ALIGN MacALIGN
#include <Cocoa/Cocoa.h>
#undef Rect
#undef ALIGN

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "platform.h"
#include "memory.h"

#include <dlfcn.h>


#define MAX_PATH PATH_MAX


#include <AppKit/AppKit.h>

void
platform_init(PlatformState* platform, SDL_SysWMinfo* sysinfo)
{

}

void
platform_deinit(PlatformState* platform)
{

}

void
platform_event_tick()
{
}

void
platform_setup_cursor(Arena* arena, PlatformState* platform)
{

}

void
platform_cursor_set_position(PlatformState* platform, v2i pos)
{
    // TODO: Implement
}

v2i
platform_cursor_get_position(PlatformState* platform)
{
    // TODO: Implement
    return v2i{};
}


void*
platform_get_gl_proc(char* name)
{
    static void* image = NULL;

    if (NULL == image) {
        image = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);
    }
    return (image ? dlsym(image, (const char *)name) : NULL);

}

char*
mac_panel(NSSavePanel *panel, FileKind kind)
{
    switch (kind) {
        case FileKind_IMAGE:
            panel.allowedFileTypes = @[(NSString*)kUTTypeJPEG, (NSString*)kUTTypePNG];
            break;
        case FileKind_MILTON_CANVAS:
            panel.allowedFileTypes = @[@"mlt"];
            break;
        default:
            break;
    }
    if ([panel runModal] != NSModalResponseOK) {
        return nullptr;
    }
    return strdup(panel.URL.path.fileSystemRepresentation);
}

NSAlert*
mac_alert(const char* info, const char* title)
{
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = [NSString stringWithUTF8String:title ? title : ""];
    alert.informativeText = [NSString stringWithUTF8String:info ? info : ""];
    return alert;
}

void
platform_dialog_mac(char* info, char* title)
{
    @autoreleasepool {
        [mac_alert(info, title) runModal];
    }
}

int32_t
platform_dialog_yesno_mac(char* info, char* title)
{
    @autoreleasepool {
        NSAlert *alert = mac_alert(info, title);
        [alert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"No", nil)];
        return [alert runModal] == NSAlertFirstButtonReturn;
    }
}

YesNoCancelAnswer
platform_dialog_yesnocancel(char* info, char* title)
{
    @autoreleasepool {
        NSAlert *alert = mac_alert(info, title);
        [alert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"No", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
        NSModalResponse result = [alert runModal];
        if (result == NSAlertFirstButtonReturn) {
          return YesNoCancelAnswer::YES_;
        }
        else if (result == NSAlertSecondButtonReturn) {
          return YesNoCancelAnswer::NO_;
        }
        return YesNoCancelAnswer::CANCEL_;
    }
}

char*
platform_open_dialog_mac(FileKind kind)
{
    @autoreleasepool {
        return mac_panel([NSOpenPanel openPanel], kind);
    }
}

char*
platform_save_dialog_mac(FileKind kind)
{
    @autoreleasepool {
        return mac_panel([NSSavePanel savePanel], kind);
    }
}

EasyTabResult
platform_handle_sysevent(PlatformState* platform, SDL_SysWMEvent* sysevent)
{
    mlt_assert(sysevent->msg->subsystem == SDL_SYSWM_COCOA);
    return EASYTAB_EVENT_NOT_HANDLED;
}

void
platform_open_link_mac(char* link)
{
    @autoreleasepool {
        NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:link]];
        [[NSWorkspace sharedWorkspace] openURL:url];
    }
}

NSWindow*
macos_get_window(PlatformState* ps)
{
    NSWindow* result = NULL;
    SDL_Window* window = ps->window;
    if (window) {
        SDL_SysWMinfo info = {};
        if (SDL_GetWindowWMInfo(window, &info)) {
            if (info.subsystem == SDL_SYSWM_COCOA) {
                NSWindow* nsw = info.info.cocoa.window;
                result = nsw;
            }
        }
    }
    return result;

}

void
platform_point_to_pixel(PlatformState* ps, v2l* inout)
{
    NSRect rect;
    rect.origin = {};
    rect.size.width = inout->w;
    rect.size.height = inout->h;
    auto* window = macos_get_window(ps);
    NSRect backing = [window convertRectToBacking:rect];

    inout->x = backing.size.width;
    inout->y = backing.size.height;
}

void
platform_point_to_pixel_i(PlatformState* ps, v2i* inout)
{
    v2l long_inout = { inout->x, inout->y };
    platform_point_to_pixel(ps, &long_inout);
    *inout = (v2i){(int)long_inout.x, (int)long_inout.y};
}

void
platform_pixel_to_point(PlatformState* ps, v2l* inout)
{
    NSRect rect;
    rect.origin = {};
    rect.size.width = inout->w;
    rect.size.height = inout->h;
    auto* window = macos_get_window(ps);
    NSRect pointrect = [window convertRectFromBacking:rect];

    inout->x = pointrect.size.width;
    inout->y = pointrect.size.height;

}

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
    extern void platform_dialog_mac(char*, char*);
    platform_dialog_mac(info, title);
    return;
}

b32
platform_dialog_yesno(char* info, char* title)
{
    extern b32 platform_dialog_yesno_mac(char*, char*);
    platform_dialog_yesno_mac(info, title);
    return false;
}

void
platform_fname_at_config(PATH_CHAR* fname, size_t len)
{
    NSBundle* bundle = [NSBundle mainBundle];
    const char* respath = [[bundle resourcePath] UTF8String];

    char* string_copy = (char*)mlt_calloc(1, len, "Strings");
    if ( string_copy ) {
        strncpy(string_copy, fname, len);

        b32 respath_failed = false;
        if (respath) {
            // Create the Resources directory if it doesn't exist.
            int mkerr = mkdir(respath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
            if (mkerr == 0) {
                milton_log("Created Resources path.\n");
            }
            else if (mkerr == -1) {
                int err = errno;
                if (err != EEXIST) {
                    milton_log("mkdir failed with unexpected error %d\n", mkerr);
                    respath_failed = true;
                }
            }
        }
        if (!respath || respath_failed) {
            char* home = getenv("HOME");
            snprintf(fname, len,  "%s/.milton", home);
            mkdir(fname, S_IRWXU);
            snprintf(fname, len,  "%s/%s", fname, string_copy);
        }
        else {
            snprintf(fname, len, "%s/%s", respath, string_copy);
        }

        mlt_free(string_copy, "Strings");
    }
}

void
platform_fname_at_exe(PATH_CHAR* fname, size_t len)
{
    u32 bufsize = (u32)len;
    char buffer[MAX_PATH] = {};
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
    FILE* fd = fopen(fname, mode);
    return fd;
}


b32
platform_move_file(PATH_CHAR* src, PATH_CHAR* dest)
{
    int res = rename(src, dest);

    return res == 0;
}

float
platform_ui_scale(PlatformState* p)
{
    int foo = 0;

    int display_w = 0;
    int framebuffer_w = 0;

    SDL_GetWindowSize(p->window, &display_w, &foo);
    SDL_GL_GetDrawableSize(p->window, &framebuffer_w, &foo);

    float scale = display_w > 0 ? framebuffer_w / display_w : 1;

    return scale;
}

PATH_CHAR*
platform_open_dialog(FileKind kind)
{
    extern PATH_CHAR* platform_open_dialog_mac(FileKind);
    return platform_open_dialog_mac(kind);
}
void
platform_open_link(char* link)
{
    extern void platform_open_link_mac(char*);
    platform_open_link_mac(link);
}
PATH_CHAR*
platform_save_dialog(FileKind kind)
{
    extern PATH_CHAR* platform_save_dialog_mac(FileKind);
    return platform_save_dialog_mac(kind);
}
//  ====

WallTime
platform_get_walltime()
{
    WallTime wt = {};
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
