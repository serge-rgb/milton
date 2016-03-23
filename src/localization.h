// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#define LOC(str) get_localized_string(TXT_ ## str)

#include "localization_txt_enums.h"

// str -- A string, translated and present in the tables within localization.c
char* get_localized_string(int id);


#if defined(__cplusplus)
}
#endif
