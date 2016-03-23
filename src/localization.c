// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "localization.h"

#include "common.h"
#include "memory.h"

enum Languages
{
    LOC_Foobar,
    LOC_English,
    LOC_Spanish,

    LOC_Count,
};

static int g_chosen_language = LOC_English;

static char* g_localized_strings[LOC_Count][TXT_Count] =
{
    { // Foo
        0
    },
    { // English
        "File",
        "Open Milton Canvas",
        "Export to Image...",
        "Quit",/* TXT_quit, */
        "Canvas",/* TXT_canvas, */
        "Set Background Color",/* TXT_set_background_color, */
        "Help",/* TXT_help, */
        "Brushes", /* TXT_brushes                        , */
        "Opacity", /* TXT_opacity                        , */
        "Brush size", /* TXT_brush_size                     , */
        "Switch to pen", /* TXT_switch_to_pen                  , */
        "Switch to eraser", /* TXT_switch_to_eraser               , */
        "Choose background color", /* TXT_choose_background_color        , */
        "Color", /* TXT_color               , */
        "Export...", /* TXT_export_DOTS                    , */
        "Click and drag to select the area to export.", /* TXT_MSG_click_and_drag_instruction , */
        "Current selection", /* TXT_current_selection              , */
        "Scale up", /* TXT_scale_up                       , */
        "Final image size", /* TXT_final_image_size               , */
        "Export selection to image...", /* TXT_export_selection_to_image_DOTS , */
        "Did not write file. Not enough memory available for operation.", /* TXT_MSG_memerr_did_not_write       , */
        "Error", /* TXT_error                          , */
        "Cancel", /* TXT_cancel                         , */
        "View",/* TXT_view                            , */
        "Toggle GUI Visibility",/* TXT_toggle_gui_visibility           , */
        "Layers",/* TXT_layers                          , */
    },
    { // Spanish
        "Archivo",/* TXT_file, */
        "Abrir Lienzo",/* TXT_open_milton_canvas, */
        "Exportar a Imagen...",/* TXT_export_to_image_DOTS, */
        "Salir",/* TXT_quit, */
        "Lienzo",/* TXT_canvas, */
        "Cambiar Color de Fondo",/* TXT_set_background_color, */
        "Ayuda",/* TXT_help, */
        "Brochas",/* TXT_brushes                        , */
        "Opacidad",/* TXT_opacity                        , */
        "Tamaño",/* TXT_brush_size                     , */
        "Usar pluma",/* TXT_switch_to_pen                  , */
        "Usar goma",/* TXT_switch_to_eraser               , */
        "Escoger color de fondo",/* TXT_choose_background_color        , */
        "Color",/* TXT_color               , */
        "Exportar...",/* TXT_export_DOTS                    , */
        "Haz click y Arrastra",/* TXT_MSG_click_and_drag_instruction , */
        "Selección actual",/* TXT_current_selection              , */
        "Escalar",/* TXT_scale_up                       , */
        "Tamaño final",/* TXT_final_image_size               , */
        "Exportar Selección a Imagen...",/* TXT_export_selection_to_image_DOTS , */
        "No se escribió archivo. No hay suficiente memoria.",/* TXT_MSG_memerr_did_not_write       , */
        "Error",/* TXT_error                          , */
        "Cancelar",/* TXT_cancel                         , */
        "Vista",/* TXT_view                            , */
        "Mostrar/Ocultar Interfaz",/* TXT_toggle_gui_visibility           , */
        "Capas",/* TXT_layers                          , */
    }
};

// Non-Mac:
//  C(x) => [Ctrl+x]
#if !defined(__MACH__)
#define C(s) "Ctrl+" s
#else
#define C(s) "CMD+" s
#endif

// Exclusively NULL pointers except for translated strings which represent a command.
static char* g_command_abbreviations[TXT_Count] =
{
    [TXT_export_to_image_DOTS] = C("E"),
    [TXT_quit]                  = "ESC",
    [TXT_toggle_gui_visibility] = "TAB",
};

#undef C

// These get malloc'd once in case that the corresponding text includes a command shortcut (see g_command_abbreviations)
static char* g_baked_strings_with_commands[TXT_Count];

// str -- A string, translated and present in the tables within localization.c
char* get_localized_string(int id)
{
    // TODO: Grab this from system
    i32 loc = LOC_English;

    char* result = g_localized_strings[loc][id];
    if ( result ) {
        char* cmd = g_command_abbreviations[id];

        // Include keyboard shortcut in string
        if ( cmd ) {
            if ( !g_baked_strings_with_commands[id] ) {
                char* name  = g_localized_strings[loc][id];
                char* spacer = " - ";

                size_t len = strlen(name) + strlen(spacer) + strlen(cmd) + 2 /*[]*/+ 1/*\n*/;
                char* with_cmd = mlt_calloc(len, 1);

                strncat_s(with_cmd, len, name, strlen(name));
                strncat_s(with_cmd, len, spacer, strlen(spacer));

                strncat_s(with_cmd, len, "[", 1);
                strncat_s(with_cmd, len, cmd, strlen(cmd));
                strncat_s(with_cmd, len, "]", 1);

                g_baked_strings_with_commands[id] = with_cmd;
            }
            result = g_baked_strings_with_commands[id];
        }
    } else {
        result = "STRING NEEDS LOCALIZATION";
    }

    return result;
}
