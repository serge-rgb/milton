#define IMPL_MISSING mlt_assert(!"IMPLEMENT")

// #define _GNU_SOURCE //temporarily targeting gcc for program_invocation_name
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>

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
    // IMPL_MISSING;
    return;
}

b32
platform_dialog_yesno(char* info, char* title)
{
    // IMPL_MISSING;
    return false;
}

void
platform_fname_at_config(PATH_CHAR* fname, size_t len)
{
    // TODO: remove this allocation
    char *string_copy = (char*)mlt_calloc(1, len, "Strings");
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
        snprintf(fname, len, "%s/%s", home, folder);
        mkdir(fname, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        strncat(fname, "/", len);
        strncat(fname, string_copy, len);
        mlt_free(string_copy, "Strings");
    }
}

void
platform_fname_at_exe(PATH_CHAR* fname, size_t len)
{
    u32 bufsize = (u32)len;
    char buffer[MAX_PATH] = {0};
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
    IMPL_MISSING;
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
