// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once


#define GLCHK(stmt) stmt; gl_query_error(#stmt, __FILE__, __LINE__)

void    gl_log(char* str);
void    gl_query_error(const char* expr, const char* file, int line);
GLuint  gl_compile_shader(const char* src, GLuint type);
void    gl_link_program(GLuint obj, GLuint shaders[], int64_t num_shaders);


