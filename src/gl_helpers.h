// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "common.h"


#define GLCHK(stmt) stmt; gl_query_error(#stmt, __FILE__, __LINE__)

#if defined(__cplusplus)
extern "C" {
#endif

void    gl_log(char* str);
void    gl_query_error(const char* expr, const char* file, int line);
GLuint  gl_compile_shader(const char* src, GLuint type);
void    gl_link_program(GLuint obj, GLuint shaders[], int64_t num_shaders);

#if defined(_WIN32)
#include "gl_func_loader.h" // GL function declarations.
#endif

#if defined(__cplusplus)
}
#endif
