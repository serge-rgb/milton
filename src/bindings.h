// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

enum BindableAction
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
   Action_HELP,

   // Debug bindings

   #if MILTON_ENABLE_PROFILING
   Action_TOGGLE_DEBUG_WINDOW,
   #endif

};

enum ModifierFlags
{
   Modifier_NONE = 0,

   Modifier_CTRL = 1<<0,
   Modifier_WIN = 1<<1,
   Modifier_ALT = 1<<2,
   Modifier_SPACE = 1<<3, // Spaaaaaace
   Modifier_SHIFT = 1<<4,
} ;

struct Binding
{
   u8 accepts_repeats;

   ModifierFlags modifiers;

   i8 bound_key;  // Positive values are ascii keys.
   // Zero/Negative values:
   enum Key
   {
      UNBOUND = 0,
      ESC = -1,

      F1 = -2,
      F2 = -3,
      F3 = -4,
      F4 = -5,
      F5 = -6,
      F6 = -7,
      F7 = -8,
      F8 = -9,
      F9 = -10,
      F10 = -11,
      F11 = -12,
      F12 = -13,
   };

   BindableAction action;
};

struct MiltonBindings
{
   #define MAX_NUM_BINDINGS 32

   size_t num_bindings;
   Binding bindings[MAX_NUM_BINDINGS];
};

// void set_default_bindings(MiltonBindings* bindings);
// void set_default_bindings(MiltonBindings* bindings, MiltonInput* input)

//void binding_dispatch_action(BindableAction a);