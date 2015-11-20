//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

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
    void set_flag(cl& f, cl val)    { f = (cl)((int)f | (int)val); }  \
    void unset_flag(cl& f, cl val)  { f = (cl)((int)f ^ (int)val); } \
    b32  check_flag(cl flags, cl f) { return (b32)((int)flags & (int)f); }

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

