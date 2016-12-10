// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#if defined(_WIN32)

#define GL_FUNCTIONS \
    X(GLint,    GetAttribLocation,      GLuint program, const GLchar* name)                     \
    X(GLuint,   CreateProgram,          void)                                                   \
    X(GLuint,   CreateShader,           GLenum type)                                            \
    X(void,     ActiveTexture,          GLenum texture)                                         \
    X(void,     AttachShader,           GLuint program, GLuint shader)                          \
    X(void,     BindAttribLocation,     GLuint program, GLuint index, const GLchar* name)       \
    X(void,     BindBuffer,             GLenum target, GLuint buffer)                           \
    X(void,     BlendEquation,          GLenum mode)                                            \
    X(void,     BlendEquationSeparate,  GLenum modeRGB, GLenum modeAlpha)                       \
    X(void,     BufferData,             GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) \
    X(void,     CompileShader,          GLuint shader)                                          \
    X(void,     DeleteBuffers,          GLsizei n, const GLuint* buffers)                       \
    X(void,     DeleteProgram,          GLuint program)                                         \
    X(void,     DeleteShader,           GLuint shader)                                          \
    X(void,     DetachShader,           GLuint program, GLuint shader)                          \
    X(void,     DisableVertexAttribArray, GLuint index)                                         \
    X(void,     EnableVertexAttribArray, GLuint index)                                          \
    X(void,     GenBuffers,             GLsizei n, GLuint *buffers)                             \
    X(void,     GetProgramInfoLog,      GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) \
    X(void,     GetProgramiv,           GLuint program, GLenum pname, GLint* params)            \
    X(void,     GetShaderInfoLog,       GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source) \
    X(void,     GetShaderiv,            GLuint shader, GLenum pname, GLint* params)             \
    X(const GLubyte*, GetStringi,       GLenum name, GLuint index)                              \
    X(GLint,    GetUniformLocation,     GLuint program, const GLchar *name)                     \
    X(GLboolean, IsProgram,             GLuint program)                                         \
    X(GLboolean, IsShader,              GLuint shader)                                          \
    X(void,     LinkProgram,            GLuint program)                                         \
    X(void,     ShaderSource,           GLuint shader, GLsizei count, const GLchar* *string, const GLint *length) \
    X(void,     Uniform1f,              GLint location, GLfloat v0)                             \
    X(void,     Uniform1i,              GLint location, GLint v0)                               \
    X(void,     Uniform2f,              GLint location, GLfloat v0, GLfloat v1)                 \
    X(void,     Uniform2fv,             GLint location, GLsizei count, const GLfloat *value )   \
    X(void,     Uniform2i,              GLint location, GLint v0, GLint v1)                     \
    X(void,     Uniform2iv,             GLint location, GLsizei count, const GLint *value )     \
    X(void,     Uniform3f,              GLint location, GLfloat v0, GLfloat v1, GLfloat v3)     \
    X(void,     Uniform3fv,             GLint location, GLsizei count, const GLfloat *value )   \
    X(void,     Uniform3i,              GLint location, GLint v0, GLint v1, GLint v3)           \
    X(void,     Uniform3iv,             GLint location, GLsizei count, const GLint *value )     \
    X(void,     Uniform4fv,             GLint location, GLsizei count, const GLfloat *value )   \
    X(void,     Uniform4iv,             GLint location, GLsizei count, const GLint *value )     \
    X(void,     UniformMatrix3fv,       GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) \
    X(void,     UniformMatrix4fv,       GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) \
    X(void,     UseProgram,             GLuint program)                                         \
    X(void,     ValidateProgram,        GLuint program)                                         \
    X(void,     VertexAttribPointer,    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer) \
    /*ARB_vertex_array_object*/\
    X(void,     GenVertexArrays,        GLsizei n, GLuint* arrays)                              \
    X(void,     DeleteVertexArrays,     GLsizei n, const GLuint* arrays)                        \
    X(void,     BindVertexArray,        GLuint array)                                           \
    /*EXT_framebuffer_object*/\
    X(void,     GenFramebuffersEXT,     GLsizei n, GLuint* framebuffers)                        \
    X(void,     BindFramebufferEXT,     GLenum target, GLuint framebuffer)                      \
    X(GLenum,   CheckFramebufferStatusEXT, GLenum target)                                       \
    X(void,     DeleteFramebuffersEXT,  GLsizei n, const GLuint *framebuffers)                  \
    X(void,     BlitFramebufferEXT,     GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) \
    X(void,     FramebufferTexture2DEXT, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) \
    /*ARB_texture_multisample*/\
    X(void,     TexImage2DMultisample,  GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) \
    X(void,     MinSampleShadingARB,    GLclampf value)


// Function prototypes.
// TODO: Does this compile on Linux?
#define X(ret, name, ...) typedef ret WINAPI name##proc(__VA_ARGS__); name##proc * gl##name##;
    GL_FUNCTIONS
#undef X

#else
    #define GL_FUNCTIONS
    #define glBlitFramebufferEXT glBlitFramebuffer
#endif  //_WIN32

bool gl_load(b32* out_supports_sample_shading)
{
#if defined(_WIN32)
#define GETADDRESS(func) { func = (decltype(func))wglGetProcAddress(#func); \
                            if ( func == NULL && strcmp("glMinSampleShadingARB", #func)!=0 )  { \
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

    // Load
#define X(ret, name, ...) GETADDRESS(gl##name)
    GL_FUNCTIONS
#undef X

#if defined(_WIN32)
#pragma warning(push, 0)
    if ( !supports_sample_shading ) {
        glMinSampleShadingARB = NULL;
    }
#pragma warning(pop)
#undef GETADDRESS
#endif
    return ok;
}

