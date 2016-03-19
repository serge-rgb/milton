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
        /* TXT_open_milton_canvas, */
        /* TXT_export_to_image_DOTS, */
        /* TXT_quit, */
        /* TXT_canvas, */
        /* TXT_set_background_color, */
        /* TXT_help, */
        /* TXT_brushes                        , */
        /* TXT_opacity                        , */
        /* TXT_brush_size                     , */
        /* TXT_switch_to_pen                  , */
        /* TXT_switch_to_eraser               , */
        /* TXT_choose_background_color        , */
        /* TXT_background_color               , */
        /* TXT_export_DOTS                    , */
        /* TXT_MSG_click_and_drag_instruction , */
        /* TXT_current_selection              , */
        /* TXT_scale_up                       , */
        /* TXT_final_image_size               , */
        /* TXT_export_selection_to_image_DOTS , */
        /* TXT_MSG_memerr_did_not_write       , */
        /* TXT_error                          , */
        /* TXT_cancel                         , */
        0,
    }
};

// str -- A string, translated and present in the tables within localization.c
char* get_localized_string(int id)
{
    //char* result = g_localized_strings[LOC_Foobar][id];
    char* result = g_localized_strings[LOC_English][id];
    if ( !result ) {
        return "STRING NEEDS LOCALIZATION";
    }
    return result;
}
