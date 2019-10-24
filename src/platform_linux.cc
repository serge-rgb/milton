// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "platform.h"

#include "common.h"
#include "memory.h"

#define IMPL_MISSING mlt_assert(!"IMPLEMENT")


extern "C" void* glXGetProcAddressARB(const GLubyte * procName);

float
perf_count_to_sec(u64 counter)
{
    // Input as nanoseconds
    return (float)counter * 1e-9;
}

u64
perf_counter()
{
    // http://stackoverflow.com/a/2660610/4717805
    timespec tp;
    int res = clock_gettime(CLOCK_REALTIME, &tp);

    // TODO: Check errno and provide more information
    if ( res ) {
        milton_log("Something went wrong with clock_gettime\n");
    }

    return tp.tv_nsec;
}

void
platform_init(PlatformState* platform, SDL_SysWMinfo* sysinfo)
{
    mlt_assert(sysinfo->subsystem == SDL_SYSWM_X11);
    gtk_init(NULL, NULL);
    EasyTab_Load(sysinfo->info.x11.display, sysinfo->info.x11.window);
}

EasyTabResult
platform_handle_sysevent(PlatformState* platform, SDL_SysWMEvent* sysevent)
{
    mlt_assert(sysevent->msg->subsystem == SDL_SYSWM_X11);
    EasyTabResult res = EasyTab_HandleEvent(&sysevent->msg->msg.x11.event);
    return res;
}

void
platform_event_tick()
{
    gtk_main_iteration_do(FALSE);
}

void
platform_point_to_pixel(PlatformState* ps, v2l* inout)
{

}

void
platform_point_to_pixel_i(PlatformState* ps, v2i* inout)
{

}

float
platform_ui_scale(PlatformState* p)
{
    return 1.0f;
}

void
platform_pixel_to_point(PlatformState* ps, v2l* inout)
{
}

b32
platform_delete_file_at_config(PATH_CHAR* fname, int error_tolerance)
{
    char fname_at_config[MAX_PATH];
    strncpy(fname_at_config, fname, MAX_PATH);
    platform_fname_at_config(fname_at_config, MAX_PATH*sizeof(char));
    int res = remove(fname_at_config);
    b32 result = true;
    if ( res != 0 ) {
        result = false;
        // Delete failed for some reason.
        if ( (error_tolerance == DeleteErrorTolerance_OK_NOT_EXIST) &&
             (errno == EEXIST || errno == ENOENT) ) {
            result = true;
        }
    }

    return result;
}

void
linux_set_GTK_filter(GtkFileChooser* chooser, GtkFileFilter* filter, FileKind kind)
{
    switch ( kind ) {
    case FileKind_IMAGE: {
            gtk_file_filter_set_name(filter, "jpeg images (.jpg, .jpeg)");
            gtk_file_filter_add_pattern(filter, "*.[jJ][pP][gG]");
            gtk_file_filter_add_pattern(filter, "*.[jJ][pP][eE][gG]");
            gtk_file_chooser_add_filter(chooser, filter);
        } break;
    case FileKind_MILTON_CANVAS: {
            gtk_file_filter_set_name(filter, "milton canvases (.mlt)");
            gtk_file_filter_add_pattern(filter, "*.[mM][lL][tT]");
            gtk_file_chooser_add_filter(chooser, filter);
        } break;
    default: {
        INVALID_CODE_PATH;
        }
    }
}

void
platform_dialog(char* info, char* title)
{
    platform_cursor_show();
    GtkWidget *dialog = gtk_message_dialog_new(
            NULL,
            (GtkDialogFlags)0,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "%s",
            info
            );
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

b32
platform_dialog_yesno(char* info, char* title)
{
    platform_cursor_show();
    GtkWidget *dialog = gtk_message_dialog_new(
            NULL,
            (GtkDialogFlags)0,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_YES_NO,
            "%s",
            info
            );
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    if ( gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_YES ) {
        gtk_widget_destroy(dialog);
        return false;
    }
    gtk_widget_destroy(dialog);
    return true;
}

YesNoCancelAnswer
platform_dialog_yesnocancel(char* info, char* title)
{
    // NOTE: As of 2019-09-23, this function hasn't been tested on Linux.

    platform_cursor_show();
    GtkWidget *dialog = gtk_message_dialog_new(
            NULL,
            (GtkDialogFlags)0,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_OK_CANCEL,
            "%s",
            info
            );
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gint answer = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if ( answer == GTK_RESPONSE_YES )
        return YesNoCancelAnswer::YES_;
    if ( answer == GTK_RESPONSE_NO )
        return YesNoCancelAnswer::NO_;
    return YesNoCancelAnswer::CANCEL_;
}

void
platform_fname_at_config(PATH_CHAR* fname, size_t len)
{
    // we'll copy fname to file_name so that we can edit fname to our liking then later append fname to it.
    char *string_copy = (char*)mlt_calloc(1, len, "Strings");
    size_t copy_length = strlen(fname);

    if ( string_copy ) {
        strncpy(string_copy, fname, len);
        char *folder;
        char *home = getenv("XDG_DATA_HOME");
        if ( !home ) {
            home = getenv("HOME");
            folder = ".milton";
        } else {
            folder = "milton";
        }
        size_t final_length = strlen(home) + strlen(folder) + copy_length + 3; // +2 for the separators /, +1 for extra null terminator
        if( final_length > len ) {
            // building a path will lead to overflow. return string_copy as it can be used as a relative path
            strncpy(fname, string_copy, len);
        } else {
            snprintf(fname, len, "%s/%s", home, folder);
            mkdir(fname, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
            strcat(fname, "/");
            strcat(fname, string_copy);
        }

        mlt_free(string_copy, "Strings");
    }
}

void
platform_fname_at_exe(PATH_CHAR* fname, size_t len)
{
    // TODO: Fix this
#if 0
    u32 bufsize = (u32)len;
    char buffer[MAX_PATH] = {};
    strncpy(buffer, fname, MAX_PATH);
    strncpy(fname, program_invocation_name, bufsize);
    {  // Remove the executable name
        PATH_CHAR* last_slash = fname;
        for( PATH_CHAR* iter = fname;
            *iter != '\0';
            ++iter ) {
            if ( *iter == '/' ) {
                last_slash = iter;
            }
        }
        *(last_slash+1) = '\0';
    }
    strncat(fname, "/", len);
    strncat(fname, buffer, len);
#endif
    return;
}

FILE*
platform_fopen(const PATH_CHAR* fname, const PATH_CHAR* mode)
{
    FILE *fd = fopen(fname, mode);
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
    platform_cursor_show();
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
            "Open File",
            NULL,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "Cancel", GTK_RESPONSE_CANCEL,
            "Open", GTK_RESPONSE_ACCEPT,
            NULL
            );
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    GtkFileFilter *filter = gtk_file_filter_new();
    linux_set_GTK_filter(chooser, filter, kind);
    gtk_file_chooser_set_current_folder(chooser, getenv("HOME"));
    if ( gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT ) {
        gtk_widget_destroy(dialog);
        return NULL;
    }
    gchar *gtk_filename = gtk_file_chooser_get_filename(chooser);
    PATH_CHAR* open_filename = (PATH_CHAR*)mlt_calloc(MAX_PATH, sizeof(PATH_CHAR), "Strings");

    PATH_STRNCPY(open_filename, gtk_filename, MAX_PATH);
    g_free(gtk_filename);
    gtk_widget_destroy(dialog);
    return open_filename;
}

void
platform_open_link(char* link)
{
    // This variant isn't safe.
    char browser[strlen(link) + 12];            //  2 quotes + 1 space + 8 'xdg-open' + 1 end
    strcpy(browser, "xdg-open '");
    strcat(browser, link);
    strcat(browser, "'");
    system(browser);
    return;
}

PATH_CHAR*
platform_save_dialog(FileKind kind)
{
    platform_cursor_show();
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
            "Save File",
            NULL,
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "Cancel", GTK_RESPONSE_CANCEL,
            "Save", GTK_RESPONSE_ACCEPT,
            NULL
            );
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    GtkFileFilter *filter = gtk_file_filter_new();
    linux_set_GTK_filter(chooser, filter, kind);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
    gtk_file_chooser_set_current_folder(chooser, getenv("HOME"));
    gtk_file_chooser_set_current_name(chooser, kind == FileKind_IMAGE ? "untitled.jpg" : "untitled.mlt");
    if ( gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT ) {
        gtk_widget_destroy(dialog);
        return NULL;
    }
    gchar *gtk_filename = gtk_file_chooser_get_filename(chooser);
    PATH_CHAR* save_filename = (PATH_CHAR*)mlt_calloc(1, MAX_PATH*sizeof(PATH_CHAR), "Strings");

    PATH_STRNCPY(save_filename, gtk_filename, MAX_PATH);
    g_free(gtk_filename);
    gtk_widget_destroy(dialog);
    return save_filename;
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

void*
platform_get_gl_proc(char* name)
{
    return glXGetProcAddressARB((GLubyte*)name);
}

void
platform_deinit(PlatformState* platform)
{

}

void
platform_setup_cursor(Arena* arena, PlatformState* platform)
{

}

v2i
platform_cursor_get_position(PlatformState* platform)
{
    v2i pos;

    SDL_GetMouseState(&pos.x, &pos.y);
    return pos;
}

void
platform_cursor_set_position(PlatformState* platform, v2i pos)
{
    SDL_WarpMouseInWindow(platform->window, pos.x, pos.y);
    // Pending mouse move events will have the cursor close
    // to where it was before we set it.
    SDL_FlushEvent(SDL_MOUSEMOTION);
    SDL_FlushEvent(SDL_SYSWMEVENT);
}
