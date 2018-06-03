#pragma once

// OpenGL defines and typedefs.
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef char GLbyte;
typedef char GLchar;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef ptrdiff_t GLsizeiptr;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef void GLvoid;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;

#include "gl_enums.inl"

#include "glext.h"

// Legacy functions.

// Modern functions.
#include "gl_functions.inl"

#if defined(_WIN32)
    // OpenGL function prototypes.
   #define X(ret, name, ...) typedef ret name##Proc(__VA_ARGS__); name##Proc * name ;
       GL_FUNCTIONS
   #undef X
#elif defined(__MACH__)
   #define X(ret, name, ...) ret name(__VA_ARGS__);
       GL_FUNCTIONS
   #undef X
#endif  //_WIN32

#ifndef GLAPI
#define GLAPI extern
#endif

