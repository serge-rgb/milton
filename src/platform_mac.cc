#define IMPL_MISSING mlt_assert(!"IMPLEMENT")



// IMPLEMENT ====
float perf_count_to_sec(u64 counter)
{
    return 0.0;
}
u64 perf_counter()
{
    return 0;
}
void*   platform_allocate_bounded_memory(size_t size)
{
    // TODO: Syscall
    return calloc(size, 1);
}
b32 platform_delete_file_at_config(PATH_CHAR* fname, int error_tolerance)
{
    return false;
}
void platform_dialog(char* info, char* title)
{
    return;
}
b32 platform_dialog_yesno(char* info, char* title)
{
    return false;
}
void platform_fname_at_config(PATH_CHAR* fname, size_t len)
{
    return;
}
void platform_fname_at_exe(PATH_CHAR* fname, size_t len)
{
    return;
}
FILE*   platform_fopen(const PATH_CHAR* fname, const PATH_CHAR* mode)
{
    return NULL;
}
b32 platform_move_file(PATH_CHAR* src, PATH_CHAR* dest)
{
    return false;
}
PATH_CHAR* platform_open_dialog(FileKind kind)
{
    return NULL;
}
void platform_open_link(char* link)
{
    return;
}
PATH_CHAR* platform_save_dialog(FileKind kind)
{
    return NULL;
}
//  ====

WallTime platform_get_walltime()
{
    WallTime wt = {0};
    IMPL_MISSING;
    return wt;
}
