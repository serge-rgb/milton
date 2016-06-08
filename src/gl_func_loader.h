
// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Functions to be loaded by platform_load_function_pointers

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

extern PFNGLACTIVETEXTUREPROC              glActiveTexture;
extern PFNGLATTACHSHADERPROC               glAttachShader;
extern PFNGLBINDATTRIBLOCATIONPROC         glBindAttribLocation;
extern PFNGLBINDBUFFERPROC                 glBindBuffer;
extern PFNGLBINDVERTEXARRAYPROC            glBindVertexArray;
extern PFNGLBLENDEQUATIONPROC              glBlendEquation;
extern PFNGLBLENDEQUATIONSEPARATEPROC      glBlendEquationSeparate;
extern PFNGLBUFFERDATAPROC                 glBufferData;
extern PFNGLCOMPILESHADERPROC              glCompileShader;
extern PFNGLCREATEPROGRAMPROC              glCreateProgram;
extern PFNGLCREATESHADERPROC               glCreateShader;
extern PFNGLDELETEBUFFERSPROC              glDeleteBuffers;
extern PFNGLDELETEPROGRAMPROC              glDeleteProgram;
extern PFNGLDELETESHADERPROC               glDeleteShader;
extern PFNGLDELETEVERTEXARRAYSPROC         glDeleteVertexArrays;
extern PFNGLDETACHSHADERPROC               glDetachShader;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC    glEnableVertexAttribArray;
extern PFNGLGENBUFFERSPROC                 glGenBuffers;
extern PFNGLGENVERTEXARRAYSPROC            glGenVertexArrays;
extern PFNGLGETATTRIBLOCATIONPROC          glGetAttribLocation;
extern PFNGLGETPROGRAMINFOLOGPROC          glGetProgramInfoLog;
extern PFNGLGETPROGRAMIVPROC               glGetProgramiv;
extern PFNGLGETSHADERINFOLOGPROC           glGetShaderInfoLog;
extern PFNGLGETSHADERIVPROC                glGetShaderiv;
extern PFNGLGETUNIFORMLOCATIONPROC         glGetUniformLocation;
extern PFNGLISPROGRAMPROC                  glIsProgram;
extern PFNGLISSHADERPROC                   glIsShader;
extern PFNGLLINKPROGRAMPROC                glLinkProgram;
extern PFNGLSHADERSOURCEPROC               glShaderSource;
extern PFNGLUNIFORM1IPROC                  glUniform1i;
extern PFNGLUNIFORM2FPROC                  glUniform2f;
extern PFNGLUNIFORM2IPROC                  glUniform2i;
extern PFNGLUNIFORM2FVPROC                 glUniform2fv;
extern PFNGLUNIFORM2IVPROC                 glUniform2iv;
extern PFNGLUNIFORM3FVPROC                 glUniform3fv;
extern PFNGLUNIFORMMATRIX3FVPROC           glUniformMatrix3fv;
extern PFNGLUNIFORMMATRIX4FVPROC           glUniformMatrix4fv;
extern PFNGLUSEPROGRAMPROC                 glUseProgram;
extern PFNGLVALIDATEPROGRAMPROC            glValidateProgram;
extern PFNGLVERTEXATTRIBPOINTERPROC        glVertexAttribPointer;

bool load_gl_functions();

#if defined(__cplusplus)
}
#endif
