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

#include "common.h"

#include "system_includes.h"


#define GLCHK(stmt) stmt; gl_query_error(#stmt, __FILE__, __LINE__)


void gl_log(char* str);
void gl_query_error(const char* expr, const char* file, int line);
GLuint gl_compile_shader(const char* src, GLuint type);
void gl_link_program(GLuint obj, GLuint shaders[], int64_t num_shaders);


