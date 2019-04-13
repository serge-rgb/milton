// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

enum BindableActions
{
   Action_DECREASE_BRUSH_SIZE,
   Action_INCREASE_BRUSH_SIZE,
   Action_ZOOM_IN,
   Action_ZOOM_OUT,
   Action_REDO,
   Action_UNDO,
   Action_EXPORT,
   Action_QUIT,
   Action_NEW,
   Action_SAVE,
   Action_SAVE_AS,
   Action_OPEN,
   Action_TOGGLE_MENU,
   Action_TOGGLE_GUI,
   Action_MODE_ERASER,
   Action_MODE_PEN,
   Action_MODE_EYEDROPPER,
   Action_MODE_PRIMITIVE,
   Action_SET_BRUSH_ALPHA_10,
   Action_SET_BRUSH_ALPHA_20,
   Action_SET_BRUSH_ALPHA_30,
   Action_SET_BRUSH_ALPHA_40,
   Action_SET_BRUSH_ALPHA_50,
   Action_SET_BRUSH_ALPHA_60,
   Action_SET_BRUSH_ALPHA_70,
   Action_SET_BRUSH_ALPHA_80,
   Action_SET_BRUSH_ALPHA_90,
   Action_SET_BRUSH_ALPHA_100,

   // Debug bindings

   #if MILTON_ENABLE_PROFILING
   Action_TOGGLE_DEBUG_WINDOW,
   #endif

};

struct KeyboardShortcut
{
   u8 accepts_repeats;
   enum
   {
      Modifier_CTRL = 1<<0,
      Modifier_WIN = 1<<1,
      Modifier_ALT = 1<<2,
      Modifier_SPACE = 1<<3, // Spaaaaaace
   } modifiers;

   i8 bound_key;  // Positive values are ascii keys.
   // Negative values:
   enum
   {
      Binding_ESC = -1,
      Binding_F1 = -2,
      Binding_F2 = -3,
      Binding_F3 = -4,
      Binding_F4 = -5,
      Binding_F5 = -6,
      Binding_F6 = -7,
      Binding_F7 = -8,
      Binding_F8 = -9,
      Binding_F9 = -10,
      Binding_F10 = -11,
      Binding_F11 = -12,
      Binding_F12 = -13,
   };
};