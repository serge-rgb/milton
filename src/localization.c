// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.


#include "localization.h"

#include "common.h"

enum Languages
{
    LOC_Foobar,
    LOC_English,
    LOC_Spanish,

    LOC_Count,
};

static int g_chosen_language = LOC_English;

static i32 g_uids[1024];

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
        "Set background color",/* TXT_set_background_color, */
        "Help",/* TXT_help, */
        "Brushes", /* TXT_brushes                        , */
        "Opacity", /* TXT_opacity                        , */
        "Brush size", /* TXT_brush_size                     , */
        "Switch to pen", /* TXT_switch_to_pen                  , */
        "Switch to eraser", /* TXT_switch_to_eraser               , */
        "Choose background color", /* TXT_choose_background_color        , */
        "Background color", /* TXT_background_color               , */
        "Export...", /* TXT_export_DOTS                    , */
        "Click and drag to select the area to export.", /* TXT_MSG_click_and_drag_instruction , */
        "Current selection", /* TXT_current_selection              , */
        "Scale up", /* TXT_scale_up                       , */
        "Final image size", /* TXT_final_image_size               , */
        "Export selection to image...", /* TXT_export_selection_to_image_DOTS , */
        "Did not write file. Not enough memory available for operation.", /* TXT_MSG_memerr_did_not_write       , */
        "Error", /* TXT_error                          , */
        "Cancel", /* TXT_cancel                         , */
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
        "Color",/* TXT_background_color               , */
        "Exportar...",/* TXT_export_DOTS                    , */
        "Haz click y Arrastra",/* TXT_MSG_click_and_drag_instruction , */
        "Selección actual",/* TXT_current_selection              , */
        "Escalar",/* TXT_scale_up                       , */
        "Tamaño final",/* TXT_final_image_size               , */
        "Exportar Selección a Imagen...",/* TXT_export_selection_to_image_DOTS , */
        "No se escribió archivo. No hay suficiente memoria.",/* TXT_MSG_memerr_did_not_write       , */
        "Error",/* TXT_error                          , */
        "Cancelar",/* TXT_cancel                         , */
    }
};

// str -- A string, translated and present in the tables within localization.c
char* get_localized_string(int id)
{
    //char* result = g_localized_strings[LOC_Foobar][id];
    char* result = g_localized_strings[LOC_English][id];
    //char* result = g_localized_strings[LOC_Spanish][id];
    if ( !result ) {
        return "STRING NEEDS LOCALIZATION";
    }
    return result;
}
