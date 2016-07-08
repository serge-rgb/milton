// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#if defined(_WIN32)
PFNGLACTIVETEXTUREPROC              glActiveTexture;
PFNGLATTACHSHADERPROC               glAttachShader;
PFNGLBINDATTRIBLOCATIONPROC         glBindAttribLocation;
PFNGLBINDBUFFERPROC                 glBindBuffer;
PFNGLBINDVERTEXARRAYPROC            glBindVertexArray;
PFNGLBLENDEQUATIONPROC              glBlendEquation;
PFNGLBLENDEQUATIONSEPARATEPROC      glBlendEquationSeparate;
PFNGLBUFFERDATAPROC                 glBufferData;
PFNGLCOMPILESHADERPROC              glCompileShader;
PFNGLCREATEPROGRAMPROC              glCreateProgram;
PFNGLCREATESHADERPROC               glCreateShader;
PFNGLDELETEBUFFERSPROC              glDeleteBuffers;
PFNGLDELETEPROGRAMPROC              glDeleteProgram;
PFNGLDELETESHADERPROC               glDeleteShader;
PFNGLDELETEVERTEXARRAYSPROC         glDeleteVertexArrays;
PFNGLDETACHSHADERPROC               glDetachShader;
PFNGLENABLEVERTEXATTRIBARRAYPROC    glEnableVertexAttribArray;
PFNGLGENBUFFERSPROC                 glGenBuffers;
PFNGLGENVERTEXARRAYSPROC            glGenVertexArrays;
PFNGLGETATTRIBLOCATIONPROC          glGetAttribLocation;
PFNGLGETPROGRAMINFOLOGPROC          glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC               glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC           glGetShaderInfoLog;
PFNGLGETSHADERIVPROC                glGetShaderiv;
PFNGLGETUNIFORMLOCATIONPROC         glGetUniformLocation;
PFNGLISPROGRAMPROC                  glIsProgram;
PFNGLISSHADERPROC                   glIsShader;
PFNGLLINKPROGRAMPROC                glLinkProgram;
PFNGLSHADERSOURCEPROC               glShaderSource;
PFNGLUNIFORM1FPROC                  glUniform1f;
PFNGLUNIFORM1IPROC                  glUniform1i;
PFNGLUNIFORM2FPROC                  glUniform2f;
PFNGLUNIFORM2IPROC                  glUniform2i;
PFNGLUNIFORM2FVPROC                 glUniform2fv;
PFNGLUNIFORM2IVPROC                 glUniform2iv;
PFNGLUNIFORM3FVPROC                 glUniform3fv;
PFNGLUNIFORM4FVPROC                 glUniform4fv;
PFNGLUNIFORMMATRIX3FVPROC           glUniformMatrix3fv;
PFNGLUNIFORMMATRIX4FVPROC           glUniformMatrix4fv;
PFNGLUSEPROGRAMPROC                 glUseProgram;
PFNGLVALIDATEPROGRAMPROC            glValidateProgram;
PFNGLVERTEXATTRIBPOINTERPROC        glVertexAttribPointer;
PFNGLVERTEXATTRIBIPOINTERPROC       glVertexAttribIPointer;
PFNGLDISABLEVERTEXATTRIBARRAYPROC   glDisableVertexAttribArray;
#endif  //_WIN32


bool load_gl_functions()
{
    bool ok = true;
#if defined(_WIN32)
#define GETADDRESS(func) if (ok) { func = (decltype(func))wglGetProcAddress(#func); \
                                    ok   = (func!=NULL); }

#pragma warning(push, 0)
    GETADDRESS(glActiveTexture);
    GETADDRESS(glAttachShader);
    GETADDRESS(glBindAttribLocation);
    GETADDRESS(glBindBuffer);
    GETADDRESS(glBindVertexArray);
    GETADDRESS(glBlendEquation);
    GETADDRESS(glBlendEquationSeparate);
    GETADDRESS(glBufferData);
    GETADDRESS(glCompileShader);
    GETADDRESS(glCreateProgram);
    GETADDRESS(glCreateShader);
    GETADDRESS(glDeleteBuffers);
    GETADDRESS(glDeleteProgram);
    GETADDRESS(glDeleteShader);
    GETADDRESS(glDeleteVertexArrays);
    GETADDRESS(glDetachShader);
    GETADDRESS(glEnableVertexAttribArray);
    GETADDRESS(glGenBuffers);
    GETADDRESS(glGenVertexArrays);
    GETADDRESS(glGetAttribLocation);
    GETADDRESS(glGetProgramInfoLog);
    GETADDRESS(glGetProgramiv);
    GETADDRESS(glGetShaderInfoLog);
    GETADDRESS(glGetShaderiv);
    GETADDRESS(glGetUniformLocation);
    GETADDRESS(glIsProgram);
    GETADDRESS(glIsShader);
    GETADDRESS(glLinkProgram);
    GETADDRESS(glShaderSource);
    GETADDRESS(glUniform1i);
    GETADDRESS(glUniform2f);
    GETADDRESS(glUniform2i);
    GETADDRESS(glUniform2fv);
    GETADDRESS(glUniform2iv);
    GETADDRESS(glUniform3fv);
    GETADDRESS(glUniform4fv);
    GETADDRESS(glUniformMatrix3fv);
    GETADDRESS(glUniformMatrix4fv);
    GETADDRESS(glUseProgram);
    GETADDRESS(glValidateProgram);
    GETADDRESS(glVertexAttribPointer);
    GETADDRESS(glVertexAttribIPointer);
    GETADDRESS(glDisableVertexAttribArray);
    GETADDRESS(glUniform1f);
#pragma warning(pop)
#undef GETADDRESS
#endif
    return ok;
}

