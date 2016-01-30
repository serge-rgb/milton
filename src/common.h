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

//
// Use enum classes as flags.
//  Insert right after definition.
//  Example:
//      enum class Flag {
//          NOTHING   = 0,
//          SOMETHING = 1,
//          OTHER     = 2,
//          ANOTHER   = 4,
//      };
//      DECLARE_FLAG(Flag);  // <--
//
//  Then you can use:
//      Flag f = NOTHING;
//      set_flag(f, Flag::SOMETHING);
//      (...)
//      if (check_flag(f, Flag::SOMETHING)) {
//          unset_flag(f, Flag::SOMETHING);
//      }
//
//
#define DECLARE_FLAG(cl) \
    inline void set_flag(cl& f, cl val)    { f = (cl)((int)f | (int)val); }  \
    inline void unset_flag(cl& f, cl val)  { f = (cl)((int)f & ~(int)val); } \
    inline b32  check_flag(cl flags, cl f) { return (b32)((int)flags & (int)f); }

//
// *** Insert at the top of all Milton containers ***
//
// Use milton's default allocator with C++ new and delete.
//
// Note:
//  In general, new and delete are Evil and Ugly, but templates are nice for
// creating new language constructs...
//
#define INSERT_ALLOC_OVERRIDES \
    static void* operator new       (std::size_t sz)    { return mlt_calloc(1, sz); }\
    static void* operator new[]     (std::size_t sz)    { return mlt_calloc(1, sz); }\
    static void  operator delete    (void* ptr)         { mlt_free(ptr); }\
    static void  operator delete[]  (void* ptr)         { mlt_free(ptr); }

//
// Declare after `private:` to avoid copying.
// A nod to pre-PC Google.
//
#define DISALLOW_EVIL_CONSTRUCTORS(TypeName)    \
   TypeName(const TypeName&);                           \
   void operator=(const TypeName&)


// Assert implementation

#if defined(assert)
//#error assert already defined
#else
#define assert(expr)  do { if ( !(bool)(expr) ) {  (*(u32*)0) = 0xDeAdBeEf;  } } while(0)
#endif
