// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#if defined(_WIN32)

// GL 2.1 ===
PFNGLACTIVETEXTUREPROC              glActiveTexture;
PFNGLATTACHSHADERPROC               glAttachShader;
PFNGLBINDATTRIBLOCATIONPROC         glBindAttribLocation;
PFNGLBINDBUFFERPROC                 glBindBuffer;
PFNGLBLENDEQUATIONPROC              glBlendEquation;
PFNGLBLENDEQUATIONSEPARATEPROC      glBlendEquationSeparate;
PFNGLBUFFERDATAPROC                 glBufferData;
//PFNGLBUFFERSUBDATAPROC              glBufferSubData;
PFNGLCOMPILESHADERPROC              glCompileShader;
PFNGLCREATEPROGRAMPROC              glCreateProgram;
PFNGLCREATESHADERPROC               glCreateShader;
PFNGLDELETEBUFFERSPROC              glDeleteBuffers;
PFNGLDELETEPROGRAMPROC              glDeleteProgram;
PFNGLDELETESHADERPROC               glDeleteShader;
PFNGLDETACHSHADERPROC               glDetachShader;
PFNGLGENBUFFERSPROC                 glGenBuffers;
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
PFNGLUNIFORM3IVPROC                 glUniform3iv;
PFNGLUNIFORM4FVPROC                 glUniform4fv;
PFNGLUNIFORMMATRIX3FVPROC           glUniformMatrix3fv;
PFNGLUNIFORMMATRIX4FVPROC           glUniformMatrix4fv;
PFNGLUSEPROGRAMPROC                 glUseProgram;
PFNGLVALIDATEPROGRAMPROC            glValidateProgram;
PFNGLENABLEVERTEXATTRIBARRAYPROC    glEnableVertexAttribArray;
PFNGLDISABLEVERTEXATTRIBARRAYPROC   glDisableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERARBPROC     glVertexAttribPointer;
//=======

#if MILTON_DEBUG
PFNGLGENVERTEXARRAYSPROC            glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC            glBindVertexArray;
PFNGLDELETEVERTEXARRAYSPROC         glDeleteVertexArrays;
#endif

// TODO: Use GL_EXT_framebuffer_blit
PFNGLBLITFRAMEBUFFERPROC            glBlitFramebuffer;


// TODO: Use GL_EXT_framebuffer_object
PFNGLGENFRAMEBUFFERSPROC      glGenFramebuffers;
PFNGLBINDFRAMEBUFFERPROC      glBindFramebuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;

// TODO: Alternative codepath for when ARB_texture_multisample extension is not present
PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;


// ARB_sample_shading
PFNGLMINSAMPLESHADINGARBPROC glMinSampleShadingARB;



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
    //GETADDRESS(glBufferSubData);
    GETADDRESS(glCompileShader);
    GETADDRESS(glCreateProgram);
    GETADDRESS(glCreateShader);
    GETADDRESS(glDeleteBuffers);
    GETADDRESS(glDeleteProgram);
    GETADDRESS(glDeleteShader);
    GETADDRESS(glDeleteVertexArrays);
    GETADDRESS(glDetachShader);
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
    GETADDRESS(glUniform3iv);
    GETADDRESS(glUniform4fv);
    GETADDRESS(glUniformMatrix3fv);
    GETADDRESS(glUniformMatrix4fv);
    GETADDRESS(glUseProgram);
    GETADDRESS(glValidateProgram);
    GETADDRESS(glUniform1f);

    GETADDRESS(glBlitFramebuffer);

    GETADDRESS(glVertexAttribPointer);
    //GETADDRESS(glVertexAttribIPointer);
    GETADDRESS(glEnableVertexAttribArray);
    GETADDRESS(glDisableVertexAttribArray);

    GETADDRESS(glGenFramebuffers);
    GETADDRESS(glBindFramebuffer);
    GETADDRESS(glFramebufferTexture2D);
    GETADDRESS(glGenRenderbuffers);
    GETADDRESS(glBindRenderbuffer);
    GETADDRESS(glRenderbufferStorage);
    GETADDRESS(glFramebufferRenderbuffer);
    GETADDRESS(glCheckFramebufferStatus);
    GETADDRESS(glDeleteFramebuffers);

    GETADDRESS(glTexImage2DMultisample);
    GETADDRESS(glFramebufferTexture2DEXT);

    GETADDRESS(glMinSampleShadingARB);
#pragma warning(pop)
#undef GETADDRESS
#endif
    return ok;
}

