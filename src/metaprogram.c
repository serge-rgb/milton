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


#include "libnuwen/meta.h"

int main()
{
    meta_clear_file("vector.generated.h");
    meta_expand(
            "vector.generated.h",
            "vector.template.h",
            4,  // Number of bindings
            // Bindings:
            "Name2", "v2f",
            "Name3", "v3f",
            "Name4", "v4f",
            "Type", "f32");
    meta_expand(
            "vector.generated.h",
            "vector.template.h",
            4,  // Number of bindings
            // Bindings:
            "Name2", "v2i",
            "Name3", "v3i",
            "Name4", "v4i",
            "Type", "i32");
    meta_expand(
            "vector.generated.h",
            "vector.template.h",
            4,  // Number of bindings
            // Bindings:
            "Name2", "v2l",
            "Name3", "v3l",
            "Name4", "v4l",
            "Type", "i64");
}
