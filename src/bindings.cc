// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "bindings.h"
#include "milton.h"

static Binding
binding(ModifierFlags mod, i8 key, BindableAction action)
{
   Binding b = {0};
   b.modifiers = mod;
   b.bound_key = key;
   b.action = action;
   return b;
}

static Binding
repeatable_binding(ModifierFlags mod, i8 key, BindableAction action)
{
   Binding b = {0};
   b.accepts_repeats = true;
   b.modifiers = mod;
   b.bound_key = key;
   b.action = action;
   return b;
}

void
set_default_bindings(MiltonBindings* bindings)
{
   Binding* b = bindings->bindings;
   b[bindings->num_bindings++] = repeatable_binding(Modifier_CTRL, 'z', Action_UNDO);
}

void
binding_dispatch_action(BindableAction a, MiltonInput* input)
{
   switch (a) {
      case Action_DECREASE_BRUSH_SIZE: {

      } break;
      case Action_INCREASE_BRUSH_SIZE: {

      } break;
      case Action_ZOOM_IN: {

      } break;
      case Action_ZOOM_OUT: {

      } break;
      case Action_REDO: {

      } break;
      case Action_UNDO: {
          input->flags |= MiltonInputFlags_UNDO;
      } break;
      case Action_EXPORT: {

      } break;
      case Action_QUIT: {

      } break;
      case Action_NEW: {

      } break;
      case Action_SAVE: {

      } break;
      case Action_SAVE_AS: {

      } break;
      case Action_OPEN: {

      } break;
      case Action_TOGGLE_MENU: {

      } break;
      case Action_TOGGLE_GUI: {

      } break;
      case Action_MODE_ERASER: {

      } break;
      case Action_MODE_PEN: {

      } break;
      case Action_MODE_EYEDROPPER: {

      } break;
      case Action_MODE_PRIMITIVE: {

      } break;
      case Action_SET_BRUSH_ALPHA_10: {

      } break;
      case Action_SET_BRUSH_ALPHA_20: {

      } break;
      case Action_SET_BRUSH_ALPHA_30: {

      } break;
      case Action_SET_BRUSH_ALPHA_40: {

      } break;
      case Action_SET_BRUSH_ALPHA_50: {

      } break;
      case Action_SET_BRUSH_ALPHA_60: {

      } break;
      case Action_SET_BRUSH_ALPHA_70: {

      } break;
      case Action_SET_BRUSH_ALPHA_80: {

      } break;
      case Action_SET_BRUSH_ALPHA_90: {

      } break;
      case Action_SET_BRUSH_ALPHA_100: {

      } break;
      default: {
         mlt_assert(!"Unhandled keyboard binding");
      }
   }
}