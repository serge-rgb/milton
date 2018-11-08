int
CALLBACK WinMain(HINSTANCE hInstance,
                 HINSTANCE hPrevInstance,
                 LPSTR lpCmdLine,
                 int nCmdShow)
{
    {
        Milton milton = {};

        PATH_CHAR* path = TO_PATH_STR("TEST_loading.mlt");
        milton_init(&milton, 0, 0, 1, path, MiltonInit_FOR_TEST);

        milton_reset_canvas_and_set_default(&milton);

        milton.persist->mlt_file_path = path;

        milton_save_v6(&milton);
    }

    {
        Milton milton = {};
        memset(g_localized_strings, 0, sizeof(g_localized_strings));

        milton_init(&milton, 0, 0, 1, TO_PATH_STR("TEST_loading.mlt"), MiltonInit_FOR_TEST);

        milton_load_v6(&milton);
    }

    return 0;
}
