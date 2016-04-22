// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "localization.h"

#include "common.h"
#include "memory.h"

enum Languages
{
    Language_FOOBAR,
    Language_ENGLISH,
    Language_SPANISH,

    Language_COUNT,
};

static int g_chosen_language = Language_ENGLISH;

static char* g_localized_strings[Language_COUNT][TXT_Count] =
{
    { // Foo
        0
    },
    { // English
        0,
        [TXT_file]                           = "File",
        [TXT_open_milton_canvas]             = "Open Milton Canvas",
        [TXT_export_to_image_DOTS]           = "Export to Image...",
        [TXT_quit]                           = "Quit",
        [TXT_canvas]                         = "Canvas",
        [TXT_set_background_color]           = "Set Background Color",
        [TXT_help]                           = "Help",
        [TXT_help_me]                        = "Help me!",
        [TXT_brushes]                        = "Brushes",
        [TXT_opacity]                        = "Opacity",
        [TXT_brush_size]                     = "Brush size",
        [TXT_switch_to_brush]                = "Switch to brush",
        [TXT_switch_to_eraser]               = "Switch to eraser",
        [TXT_choose_background_color]        = "Choose background color",
        [TXT_color]                          = "Color",
        [TXT_export_DOTS]                    = "Export...",
        [TXT_MSG_click_and_drag_instruction] = "Click and drag to select the area to export.",
        [TXT_current_selection]              = "Current selection",
        [TXT_scale_up]                       = "Scale up",
        [TXT_final_image_size]               = "Final image size",
        [TXT_export_selection_to_image_DOTS] = "Export selection to image...",
        [TXT_MSG_memerr_did_not_write]       = "Did not write file. Not enough memory available for operation.",
        [TXT_error]                          = "Error",
        [TXT_cancel]                         = "Cancel",
        [TXT_view]                           = "View",
        [TXT_toggle_gui_visibility]          = "Toggle GUI Visibility",
        [TXT_layers]                         = "Layers",
        [TXT_new_layer]                      = "New Layer",
        [TXT_rename]                         = "Rename",
        [TXT_move]                           = "Move",
        [TXT_move_canvas]                    = "Move canvas",
        [TXT_stop_moving_canvas]             = "Stop moving",
        [TXT_up]                             = "Up",
        [TXT_down]                           = "Down",
        [TXT_are_you_sure]                   = "Are you sure?",
        [TXT_cant_be_undone]                 = "Can't be undone",
        [TXT_yes]                            = "Yes",
        [TXT_no]                             = "No",
        [TXT_ok]                             = "OK",
        [TXT_delete]                         = "Delete",
        [TXT_edit]                           = "Edit",
        [TXT_undo]                           = "Undo",
        [TXT_redo]                           = "Redo",
        [TXT_tools]                          = "Tools",
        [TXT_brush]                          = "Brush",
        [TXT_eraser]                         = "Eraser",
        [TXT_zoom_in]                        = "Zoom In",
        [TXT_zoom_out]                       = "Zoom Out",
        [TXT_brush_options]                  = "Brush Options",
        [TXT_set_opacity_to]                 = "Set brush opacity to",
        [TXT_save_milton_canvas_as_DOTS]     = "Save Milton Canvas As...",
        [TXT_new_milton_canvas]              = "New Milton Canvas",
        [TXT_decrease_brush_size]            = "Decrease Brush Size",
        [TXT_increase_brush_size]            = "Increase Brush Size",
        [TXT_eye_dropper]                    = "Eye Dropper",
    },

    { // Spanish
        0,
        [TXT_file]                           = "Archivo",
        [TXT_open_milton_canvas]             = "Abrir Lienzo",
        [TXT_export_to_image_DOTS]           = "Exportar a Imagen...",
        [TXT_quit]                           = "Salir",
        [TXT_canvas]                         = "Lienzo",
        [TXT_set_background_color]           = "Cambiar Color de Fondo",
        [TXT_help]                           = "Ayuda",
        [TXT_brushes]                        = "Brochas",
        [TXT_opacity]                        = "Opacidad",
        [TXT_brush_size]                     = "Tamaño",
        [TXT_switch_to_brush]                = "Usar brocha",
        [TXT_switch_to_eraser]               = "Usar goma",
        [TXT_choose_background_color]        = "Escoger color de fondo",
        [TXT_color]                          = "Color",
        [TXT_export_DOTS]                    = "Exportar...",
        [TXT_MSG_click_and_drag_instruction] = "Haz click y Arrastra",
        [TXT_current_selection]              = "Selección actual",
        [TXT_scale_up]                       = "Escalar",
        [TXT_final_image_size]               = "Tamaño final",
        [TXT_export_selection_to_image_DOTS] = "Exportar Selección a Imagen...",
        [TXT_MSG_memerr_did_not_write]       = "No se escribió archivo. No hay suficiente memoria.",
        [TXT_error]                          = "Error",
        [TXT_cancel]                         = "Cancelar",
        [TXT_view]                           = "Vista",
        [TXT_toggle_gui_visibility]          = "Mostrar/Ocultar Interfaz",
        [TXT_layers]                         = "Capas",
    }
};

// Non-Mac:
//  C(x) => [Ctrl+x]
#if !defined(__MACH__)
#define C(s) "Ctrl+" s
#else
#define C(s) "CMD+" s
#endif

// TODO: Translate keys...
// Exclusively NULL pointers except for translated strings which represent a command.
static char* g_command_abbreviations[TXT_Count] =
{
    [TXT_export_to_image_DOTS]  = C("E"),
    [TXT_quit]                  = "ESC",
    [TXT_toggle_gui_visibility] = "TAB",
    [TXT_brush]                 = "B",
    [TXT_eraser]                = "E",
    [TXT_undo]                  = C("Z"),
    [TXT_redo]                  = C("Shift+Z"),
    [TXT_zoom_in]               = C(" +"),
    [TXT_zoom_out]              = C(" -"),
    [TXT_move_canvas]           = "SPACE",
    [TXT_stop_moving_canvas]    = "SPACE",
    [TXT_decrease_brush_size]   = C("]"),
    [TXT_increase_brush_size]   = C("["),
    [TXT_eye_dropper]           = "i",
    [TXT_switch_to_brush]       = "B",
    [TXT_switch_to_eraser]      = "E",
};

#undef C

// These get malloc'd once in case that the corresponding text includes a command shortcut (see g_command_abbreviations)
static char* g_baked_strings_with_commands[TXT_Count];

// str -- A string, translated and present in the tables within localization.c
char* get_localized_string(int id)
{
    // TODO: Grab this from system
    i32 loc = Language_ENGLISH;

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
