// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "bindings.h"
#include "milton.h"
#include "gui.h"
#include "persist.h"


static void
binding(MiltonBindings* bs, ModifierFlags mod, i8 key, BindableAction action)
{
    Binding b = {0};
    b.modifiers = mod;
    b.bound_key = key;
    b.action = action;

    bs->bindings[action] = b;
}

static void
repeatable_binding(MiltonBindings* bs, ModifierFlags mod, i8 key, BindableAction action)
{
    Binding b = {0};
    b.accepts_repeats = true;
    b.modifiers = mod;
    b.bound_key = key;
    b.action = action;

    bs->bindings[action] = b;
}

static void
binding_with_release(MiltonBindings* bs, ModifierFlags mod, i8 key, BindableAction pressed, BindableAction release)
{
    Binding b = {0};

    b.modifiers = mod;
    b.bound_key = key;
    b.action = pressed;

    bs->bindings[pressed] = b;

    b.action = release;
    b.on_release = true;
    bs->bindings[release] = b;
}


void
set_default_bindings(MiltonBindings* bs)
{
    Binding* b = bs->bindings;
    repeatable_binding(bs, Modifier_CTRL, 'z', Action_UNDO);
    repeatable_binding(bs, (ModifierFlags)(Modifier_CTRL | Modifier_SHIFT), 'z', Action_REDO);
    repeatable_binding(bs, Modifier_NONE, '[', Action_DECREASE_BRUSH_SIZE);
    repeatable_binding(bs, Modifier_NONE, ']', Action_INCREASE_BRUSH_SIZE);
    repeatable_binding(bs, Modifier_CTRL, '=', Action_ZOOM_IN);
    repeatable_binding(bs, Modifier_CTRL, '-', Action_ZOOM_OUT);

    binding(bs, Modifier_CTRL, '-', Action_ZOOM_OUT);
    binding(bs, Modifier_CTRL, 'e', Action_EXPORT);
    binding(bs, Modifier_CTRL, 'q', Action_QUIT);
    binding(bs, Modifier_CTRL, 'n', Action_NEW);
    binding(bs, Modifier_CTRL, 'o', Action_OPEN);
    binding(bs, (ModifierFlags)(Modifier_CTRL | Modifier_SHIFT), 's', Action_SAVE_AS);

    binding(bs, Modifier_NONE, 'm', Action_TOGGLE_MENU);
    binding(bs, Modifier_NONE, 'e', Action_MODE_ERASER);
    binding(bs, Modifier_NONE, 'b', Action_MODE_PEN);
    binding(bs, Modifier_NONE, 'i', Action_MODE_EYEDROPPER);
    binding(bs, Modifier_NONE, 'l', Action_MODE_PRIMITIVE);
    binding(bs, Modifier_NONE, Binding::F1, Action_HELP);
    binding(bs, Modifier_NONE, Binding::TAB, Action_TOGGLE_GUI);

    binding(bs, Modifier_NONE, '1', Action_SET_BRUSH_ALPHA_10);
    binding(bs, Modifier_NONE, '2', Action_SET_BRUSH_ALPHA_20);
    binding(bs, Modifier_NONE, '3', Action_SET_BRUSH_ALPHA_30);
    binding(bs, Modifier_NONE, '4', Action_SET_BRUSH_ALPHA_40);
    binding(bs, Modifier_NONE, '5', Action_SET_BRUSH_ALPHA_50);
    binding(bs, Modifier_NONE, '6', Action_SET_BRUSH_ALPHA_60);
    binding(bs, Modifier_NONE, '7', Action_SET_BRUSH_ALPHA_70);
    binding(bs, Modifier_NONE, '8', Action_SET_BRUSH_ALPHA_80);
    binding(bs, Modifier_NONE, '9', Action_SET_BRUSH_ALPHA_90);
    binding(bs, Modifier_NONE, '0', Action_SET_BRUSH_ALPHA_100);

    binding_with_release(bs, Modifier_NONE, '`', Action_PEEK_OUT, ActionRelease_PEEK_OUT);
    binding_with_release(bs, Modifier_SHIFT, Binding::UNBOUND, Action_DRAG_BRUSH_SIZE, ActionRelease_DRAG_BRUSH_SIZE);
    binding_with_release(bs, Modifier_ALT, Binding::UNBOUND, Action_TRANSFORM, ActionRelease_TRANSFORM);

    #if MILTON_ENABLE_PROFILING
    binding(bs, Modifier_CTRL, '`', Action_TOGGLE_DEBUG_WINDOW);
    #endif
}

void
binding_dispatch_action(BindableAction a, MiltonInput* input, Milton* milton, v2i pointer)
{
    char* default_will_be_lost = "The default canvas will be cleared. Save it?";

    switch (a) {
        case Action_DECREASE_BRUSH_SIZE: {
            milton_decrease_brush_size(milton);
        } break;
        case Action_INCREASE_BRUSH_SIZE: {
            milton_increase_brush_size(milton);
        } break;
        case Action_ZOOM_IN: {
            input->scale++;
            milton_set_zoom_at_screen_center(milton);
        } break;
        case Action_ZOOM_OUT: {
            input->scale--;
            milton_set_zoom_at_screen_center(milton);
        } break;
        case Action_REDO: {
            input->flags |= MiltonInputFlags_REDO;
        } break;
        case Action_UNDO: {
            input->flags |= MiltonInputFlags_UNDO;
        } break;
        case Action_EXPORT: {
            input->mode_to_set = MiltonMode::EXPORTING;
        } break;
        case Action_QUIT: {
            milton_try_quit(milton);
        } break;
        case Action_NEW: {
            YesNoCancelAnswer save_file = YesNoCancelAnswer::NO_;
            if ( layer::count_strokes(milton->canvas->root_layer) > 0 ) {
                if ( milton->flags & MiltonStateFlags_DEFAULT_CANVAS ) {
                    save_file = platform_dialog_yesnocancel(default_will_be_lost, "Save?");
                }
            }
            if ( save_file == YesNoCancelAnswer::CANCEL_ )
                break;
            if ( save_file == YesNoCancelAnswer::YES_ ) {
                PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                if ( !name ) // save dialog was cancelled
                    break;
                milton_log("Saving to %s\n", name);
                milton_set_canvas_file(milton, name);
                milton_save(milton);
                b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"), DeleteErrorTolerance_OK_NOT_EXIST);
                if ( del == false ) {
                    platform_dialog("Could not delete contents. The work will be still be there even though you saved it to a file.",
                        "Info");
                }
            }
            milton_reset_canvas_and_set_default(milton);
            input->flags |= MiltonInputFlags_FULL_REFRESH;
            milton->flags |= MiltonStateFlags_DEFAULT_CANVAS;
        } break;
        case Action_SAVE: {
            INVALID_CODE_PATH;
        } break;
        case Action_SAVE_AS: {
            PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
            if ( name ) {
                milton_log("Saving to %s\n", name);
                milton_set_canvas_file(milton, name);
                input->flags |= MiltonInputFlags_SAVE_FILE;
                b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"),
                    DeleteErrorTolerance_OK_NOT_EXIST);
                if ( del == false ) {
                    platform_dialog("Could not delete default canvas. Contents will be still there when you create a new canvas.",
                        "Info");
                }
            }
        } break;
        case Action_OPEN: {
            b32 save_requested = false;
            // If current canvas is MiltonPersist, then prompt to save
            if ( ( milton->flags & MiltonStateFlags_DEFAULT_CANVAS ) ) {
                b32 save_file = false;
                if ( layer::count_strokes(milton->canvas->root_layer) > 0 ) {
                    save_file = platform_dialog_yesno(default_will_be_lost, "Save?");
                }
                if ( save_file ) {
                    PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                    if ( name ) {
                        milton_log("Saving to %s\n", name);
                        milton_set_canvas_file(milton, name);
                        milton_save(milton);
                        b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"),
                            DeleteErrorTolerance_OK_NOT_EXIST);
                        if ( del == false ) {
                            platform_dialog("Could not delete default canvas. Contents will be still there when you create a new canvas.",
                                "Info");
                        }
                    }
                }
            }
            PATH_CHAR* fname = platform_open_dialog(FileKind_MILTON_CANVAS);
            if ( fname ) {
                milton_set_canvas_file(milton, fname);
                input->flags |= MiltonInputFlags_OPEN_FILE;
            }
        } break;
        case Action_TOGGLE_MENU: {
            gui_toggle_menu_visibility(milton->gui);
        } break;
        case Action_TOGGLE_GUI: {
            milton_toggle_gui_visibility(milton);
        } break;
        case Action_MODE_ERASER: {
            input->mode_to_set = MiltonMode::ERASER;
        } break;
        case Action_MODE_PEN: {
            input->mode_to_set = MiltonMode::PEN;
        } break;
        case Action_MODE_EYEDROPPER: {
            input->mode_to_set = MiltonMode::EYEDROPPER;
        } break;
        case Action_MODE_PRIMITIVE: {
            input->mode_to_set = MiltonMode::PRIMITIVE;
        } break;
        case Action_SET_BRUSH_ALPHA_10: {
            milton_set_brush_alpha(milton, 0.1f);
        } break;
        case Action_SET_BRUSH_ALPHA_20: {
            milton_set_brush_alpha(milton, 0.2f);
        } break;
        case Action_SET_BRUSH_ALPHA_30: {
            milton_set_brush_alpha(milton, 0.3f);
        } break;
        case Action_SET_BRUSH_ALPHA_40: {
            milton_set_brush_alpha(milton, 0.4f);
        } break;
        case Action_SET_BRUSH_ALPHA_50: {
            milton_set_brush_alpha(milton, 0.5f);
        } break;
        case Action_SET_BRUSH_ALPHA_60: {
            milton_set_brush_alpha(milton, 0.6f);
        } break;
        case Action_SET_BRUSH_ALPHA_70: {
            milton_set_brush_alpha(milton, 0.7f);
        } break;
        case Action_SET_BRUSH_ALPHA_80: {
            milton_set_brush_alpha(milton, 0.8f);
        } break;
        case Action_SET_BRUSH_ALPHA_90: {
            milton_set_brush_alpha(milton, 0.9f);
        } break;
        case Action_SET_BRUSH_ALPHA_100: {
            milton_set_brush_alpha(milton, 1.0);
        } break;
        case Action_HELP: {
            gui_toggle_help(milton->gui);
        } break;
        case Action_PEEK_OUT: {
            peek_out_trigger_start(milton);
        } break;
        case ActionRelease_PEEK_OUT: {
            peek_out_trigger_stop(milton);
        } break;
        case Action_DRAG_BRUSH_SIZE: {
            drag_brush_size_start(milton, pointer);
        } break;
        case ActionRelease_DRAG_BRUSH_SIZE: {
            drag_brush_size_stop(milton);
        } break;
        case Action_TRANSFORM: {
            transform_start(milton, pointer);
        } break;
        case ActionRelease_TRANSFORM: {
            transform_stop(milton);
        } break;
#if MILTON_ENABLE_PROFILING
        case Action_TOGGLE_DEBUG_WINDOW: {
            milton->viz_window_visible = !milton->viz_window_visible;
        } break;
#endif
        default: {
            mlt_assert(!"Unhandled keyboard binding");
        }
    }
}
