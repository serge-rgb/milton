#pragma once

// OpenGL defines and typedefs.
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef char GLchar;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
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

#include "gl_functions.inl"

#define GL_DEBUG_CALLBACK(name)  void name(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)

typedef GL_DEBUG_CALLBACK(GlDebugCallback);

// OpenGL function prototypes.
#define X(ret, name, ...) typedef ret name##Proc(__VA_ARGS__); name##Proc * name ;
    GL_FUNCTIONS
#undef X

#ifndef GLAPI
#define GLAPI extern
#endif

