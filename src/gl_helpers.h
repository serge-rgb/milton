// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once


#if MILTON_DEBUG
#define GLCHK(stmt) stmt; gl_query_error(#stmt, __FILE__, __LINE__)
#else
#define GLCHK(stmt) stmt;
#endif

void    gl_log(char* str);
void    gl_query_error(const char* expr, const char* file, int line);
GLuint  gl_compile_shader(const char* src, GLuint type, char* config = "");
void    gl_link_program(GLuint obj, GLuint shaders[], int64_t num_shaders);

// TODO: When porting to OSX, check if this is still necessary.
#if 0
#if defined(__MACH__)
#define glGetAttribLocationARB glGetAttribLocation
#define glGetUniformLocationARB glGetUniformLocation
#define glUseProgramObjectARB glUseProgram
#define glCreateProgramObjectARB glCreateProgram
#define glEnableVertexAttribArrayARB glEnableVertexAttribArray
#define glVertexAttribPointerARB glVertexAttribPointer
#define glGenVertexArrays glGenVertexArraysAPPLE
#define glBindVertexArray glBindVertexArrayAPPLE
#endif
#endif