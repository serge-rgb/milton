// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#if defined(_WIN32)

// OpenGL 2.1 ===
PFNGLACTIVETEXTUREPROC              glActiveTexture;
PFNGLATTACHSHADERPROC               glAttachShader;
PFNGLBINDATTRIBLOCATIONPROC         glBindAttribLocation;
PFNGLBINDBUFFERPROC                 glBindBuffer;
PFNGLBLENDEQUATIONPROC              glBlendEquation;
PFNGLBLENDEQUATIONSEPARATEPROC      glBlendEquationSeparate;
PFNGLBUFFERDATAPROC                 glBufferData;
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
PFNGLGETSTRINGIPROC                 glGetStringi;
//PFNGLBUFFERSUBDATAPROC              glBufferSubData;

// OpenGL 3.0+
#if USE_GL_3_2
PFNGLGENVERTEXARRAYSPROC            glGenVertexArrays;
PFNGLDELETEVERTEXARRAYSPROC         glDeleteVertexArrays;
PFNGLBINDVERTEXARRAYPROC            glBindVertexArray;
#endif

PFNGLGENFRAMEBUFFERSEXTPROC            glGenFramebuffersEXT;
PFNGLBINDFRAMEBUFFEREXTPROC            glBindFramebufferEXT;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC       glFramebufferTexture2DEXT;
PFNGLGENRENDERBUFFERSEXTPROC           glGenRenderbuffersEXT;
PFNGLBINDRENDERBUFFEREXTPROC           glBindRenderbufferEXT;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC     glCheckFramebufferStatusEXT;
PFNGLDELETEFRAMEBUFFERSEXTPROC         glDeleteFramebuffersEXT;
PFNGLBLITFRAMEBUFFEREXTPROC            glBlitFramebufferEXT;
// PFNGLRENDERBUFFERSTORAGEPROC        glRenderbufferStorage;
// PFNGLFRAMEBUFFERRENDERBUFFERPROC    glFramebufferRenderbuffer;


// OpenGL 3.2+
PFNGLTEXIMAGE2DMULTISAMPLEPROC      glTexImage2DMultisample;

// ARB_sample_shading
PFNGLMINSAMPLESHADINGARBPROC        glMinSampleShadingARB;

#endif  //_WIN32


bool gl_load(b32* out_supports_sample_shading)
{
#if defined(_WIN32)
#define GETADDRESS(func) { func = (decltype(func))wglGetProcAddress(#func); \
                            if (func == NULL) { \
                                char* msg = "Could not load function " #func; \
                                milton_log(msg);\
                                milton_die_gracefully(msg); \
                            } \
                        }
#else
#define GETADDRESS(func)  // Ignore
#endif

    bool ok = true;
    // Extension checking.
    i64 num_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, (GLint*)&num_extensions);

    GETADDRESS(glGetStringi);
    b32 supports_sample_shading = false;

    if ( &glGetStringi ) {
        for ( i64 extension_i = 0; extension_i < num_extensions; ++extension_i ) {
            char* extension_string = (char*)glGetStringi(GL_EXTENSIONS, (GLuint)extension_i);

            if ( strcmp(extension_string, "GL_ARB_sample_shading") == 0 ) {
                supports_sample_shading = true;
            }
        }
    }

    if ( out_supports_sample_shading ) {
        *out_supports_sample_shading = supports_sample_shading;
    }

#if defined(_WIN32)
#pragma warning(push, 0)
    GETADDRESS(glActiveTexture);
    GETADDRESS(glAttachShader);
    GETADDRESS(glBindAttribLocation);
    GETADDRESS(glBindBuffer);
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
    GETADDRESS(glDetachShader);
    GETADDRESS(glGenBuffers);
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
    GETADDRESS(glVertexAttribPointer);
    GETADDRESS(glEnableVertexAttribArray);
    GETADDRESS(glDisableVertexAttribArray);

#if USE_GL_3_2
    GETADDRESS(glGenVertexArrays);
    GETADDRESS(glDeleteVertexArrays);
    GETADDRESS(glBindVertexArray);
#endif

    GETADDRESS(glBlitFramebufferEXT);

    GETADDRESS(glGenFramebuffersEXT);
    GETADDRESS(glBindFramebufferEXT);
    GETADDRESS(glFramebufferTexture2DEXT);
    GETADDRESS(glGenRenderbuffersEXT);
    GETADDRESS(glBindRenderbufferEXT);
    GETADDRESS(glCheckFramebufferStatusEXT);
    GETADDRESS(glDeleteFramebuffersEXT);

    GETADDRESS(glTexImage2DMultisample);

    if ( supports_sample_shading ) {
        GETADDRESS(glMinSampleShadingARB);
    } else {
        glMinSampleShadingARB = NULL;
    }
#pragma warning(pop)
#undef GETADDRESS
#endif
    return ok;
}

