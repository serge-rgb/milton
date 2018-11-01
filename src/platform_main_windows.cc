// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

int
CALLBACK WinMain(HINSTANCE hInstance,
                 HINSTANCE hPrevInstance,
                 LPSTR lpCmdLine,
                 int nCmdShow)
{
    win32_cleanup_appdata();
    PATH_CHAR path[MAX_PATH] = TO_PATH_STR("milton.log");
    platform_fname_at_config(path, MAX_PATH);
    g_win32_logfile = platform_fopen(path, TO_PATH_STR("w"));
    char cmd_line[MAX_PATH] = {};
    strncpy(cmd_line, lpCmdLine, MAX_PATH);

    bool is_fullscreen = false;
    //TODO: proper cmd parsing
    if ( cmd_line[0] == '-' && cmd_line[1] == 'F' && cmd_line[2] == ' ' ) {
        is_fullscreen = true;
        milton_log("Fullscreen is set.\n");
        for ( size_t i = 0; cmd_line[i]; ++i) {
            if ( cmd_line[i + 3] == '\0') {
                cmd_line[i] = '\0';
                break;
            }
            else {
                cmd_line[i] = cmd_line[i + 3];
            }
        }
    }
    else if ( cmd_line[0] == '-' && cmd_line[1] == 'F' ) {
        is_fullscreen = true;
        milton_log("Fullscreen is set.\n");
        for ( size_t i = 0; cmd_line[i]; ++i) {
            if ( cmd_line[i + 2] == '\0') {
                cmd_line[i] = '\0';
            }
            else {
                cmd_line[i] = cmd_line[i + 2];
            }
        }
    }

    if ( cmd_line[0] == '"' && cmd_line[strlen(cmd_line)-1] == '"' ) {
        for ( size_t i = 0; cmd_line[i]; ++i ) {
            cmd_line[i] = cmd_line[i+1];
        }
        size_t sz = strlen(cmd_line);
        cmd_line[sz-1] = '\0';
    }

    // TODO: eat spaces
    char* file_to_open = NULL;
    milton_log("CommandLine is %s\n", cmd_line);
    if ( strlen(cmd_line) != 0 ) {
        file_to_open = cmd_line;
    }
    milton_main(is_fullscreen, file_to_open);
}
