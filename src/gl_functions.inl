// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#define GL_FUNCTIONS \
    X(GLint,    glGetAttribLocation,      GLuint program, const GLchar* name)                     \
    X(GLuint,   glCreateProgram,          void)                                                   \
    X(GLuint,   glCreateShader,           GLenum type)                                            \
    X(void,     glActiveTexture,          GLenum texture)                                         \
    X(void,     glAttachShader,           GLuint program, GLuint shader)                          \
    X(void,     glBindAttribLocation,     GLuint program, GLuint index, const GLchar* name)       \
    X(void,     glBindBuffer,             GLenum target, GLuint buffer)                           \
    X(void,     glBlendEquation,          GLenum mode)                                            \
    X(void,     glBlendEquationSeparate,  GLenum modeRGB, GLenum modeAlpha)                       \
    X(void,     glBufferData,             GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) \
    X(void,     glCompileShader,          GLuint shader)                                          \
    X(void,     glDeleteBuffers,          GLsizei n, const GLuint* buffers)                       \
    X(void,     glDeleteProgram,          GLuint program)                                         \
    X(void,     glDeleteShader,           GLuint shader)                                          \
    X(void,     glDetachShader,           GLuint program, GLuint shader)                          \
    X(void,     glDisableVertexAttribArray, GLuint index)                                         \
    X(void,     glEnableVertexAttribArray, GLuint index)                                          \
    X(void,     glGenBuffers,             GLsizei n, GLuint *buffers)                             \
    X(void,     glGetProgramInfoLog,      GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) \
    X(void,     glGetProgramiv,           GLuint program, GLenum pname, GLint* params)            \
    X(void,     glGetShaderInfoLog,       GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source) \
    X(void,     glGetShaderiv,            GLuint shader, GLenum pname, GLint* params)             \
    X(const GLubyte*, glGetStringi,       GLenum name, GLuint index)                              \
    X(GLint,    glGetUniformLocation,     GLuint program, const GLchar *name)                     \
    X(GLboolean, glIsProgram,             GLuint program)                                         \
    X(GLboolean, glIsShader,              GLuint shader)                                          \
    X(void,     glLinkProgram,            GLuint program)                                         \
    X(void,     glShaderSource,           GLuint shader, GLsizei count, const GLchar* *string, const GLint *length) \
    X(void,     glUniform1f,              GLint location, GLfloat v0)                             \
    X(void,     glUniform1i,              GLint location, GLint v0)                               \
    X(void,     glUniform2f,              GLint location, GLfloat v0, GLfloat v1)                 \
    X(void,     glUniform2fv,             GLint location, GLsizei count, const GLfloat *value )   \
    X(void,     glUniform2i,              GLint location, GLint v0, GLint v1)                     \
    X(void,     glUniform2iv,             GLint location, GLsizei count, const GLint *value )     \
    X(void,     glUniform3f,              GLint location, GLfloat v0, GLfloat v1, GLfloat v3)     \
    X(void,     glUniform3fv,             GLint location, GLsizei count, const GLfloat *value )   \
    X(void,     glUniform3i,              GLint location, GLint v0, GLint v1, GLint v3)           \
    X(void,     glUniform3iv,             GLint location, GLsizei count, const GLint *value )     \
    X(void,     glUniform4fv,             GLint location, GLsizei count, const GLfloat *value )   \
    X(void,     glUniform4iv,             GLint location, GLsizei count, const GLint *value )     \
    X(void,     glUniformMatrix3fv,       GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) \
    X(void,     glUniformMatrix4fv,       GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) \
    X(void,     glUseProgram,             GLuint program)                                         \
    X(void,     glValidateProgram,        GLuint program)                                         \
    X(void,     glVertexAttribPointer,    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer) \
    /*ARB_vertex_array_object*/\
    X(void,     glGenVertexArrays,        GLsizei n, GLuint* arrays)                              \
    X(void,     glDeleteVertexArrays,     GLsizei n, const GLuint* arrays)                        \
    X(void,     glBindVertexArray,        GLuint array)                                           \
    /*ARB_framebuffer_object*/\
    X(void,     glGenFramebuffers,     GLsizei n, GLuint* framebuffers)                        \
    X(void,     glBindFramebuffer,     GLenum target, GLuint framebuffer)                      \
    X(GLenum,   glCheckFramebufferStatus, GLenum target)                                       \
    X(void,     glDeleteFramebuffers,  GLsizei n, const GLuint *framebuffers)                  \
    X(void,     glFramebufferTexture2D, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) \
    /*EXT_framebuffer_blit*/\
    X(void,     glBlitFramebufferEXT,     GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) \
    /*ARB_texture_multisample*/\
    X(void,     glTexImage2DMultisample,  GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) \
    X(void,     glDebugMessageCallback, GlDebugCallback callback, void* userparam ) \
    /* glMinSampleShadingARB gets treated separately because Milton can handle it missing. */ \
    /*X(void,     glMinSampleShadingARB,    GLclampf value)*/
