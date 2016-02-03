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


#pragma once

#include <stdint.h>

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef float       f32;

typedef i32         b32;


#define MILTON_DESKTOP
#include "system_includes.h"

#ifdef UNUSED
#error "Someone else defined UNUSED macro"
#else
#define UNUSED(x) (void*)(&x)
#endif

// Assert implementation

#if defined(assert)
//#error assert already defined
#else
#define assert(expr)  do { if ( !(bool)(expr) ) {  (*(u32*)0) = 0xDeAdBeEf;  } } while(0)
#endif

#define check_flag(flags, check)    ((flags) & (check))
#define set_flag(f, s)              ((f) |= (s))
#define unset_flag(f, s)            ((f) &= ~(s))
