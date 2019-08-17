// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#if MILTON_DEBUG
#define GL_FUNCTIONS_DEBUG \
    X(void, glDebugMessageCallback, GlDebugCallback callback, void* user_param)
#else
    #define GL_FUNCTIONS_DEBUG
#endif // MILTON_DEBUG

#if GRAPHICS_DEBUG
    #define GL_FUNCTIONS_GRAPHICS_DEBUG \
        X(void, glPushDebugGroup, GLenum source, GLuint id, GLsizei length, const char * message) \
        X(void, glPopDebugGroup)
#else
    #define GL_FUNCTIONS_GRAPHICS_DEBUG
#endif

#define GL_FUNCTIONS_CORE \
    X(GLenum,   glGetError, void)\
    X(GLint,    glGetAttribLocation,      GLuint program, GLchar* name)                     \
    X(GLint,    glGetUniformLocation,     GLuint program, GLchar *name)                     \
    X(const GLubyte *, glGetString, GLenum name )\
    X(const GLubyte*, glGetStringi,       GLenum name, GLuint index)                              \
    X(void,     glBindFramebufferEXT,     GLenum target, GLuint framebuffer)                      \
    X(void,     glBindTexture,            GLenum target, GLuint text) \
    X(void,     glBufferData,             GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) \
    X(void,     glCompileShader,          GLuint shader)                                          \
    X(void,     glEnable, GLenum cap )\
    X(void,     glFramebufferTexture2DEXT, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) \
    X(void,     glGenFramebuffersEXT,     GLsizei n, GLuint* framebuffers)                        \
    X(void,     glGenTextures,            GLsizei n, GLuint* textures) \
    X(void,     glAttachShader,           GLuint program, GLuint shader)                          \
    X(GLboolean, glIsProgram,             GLuint program)                                         \
    X(GLboolean, glIsShader,              GLuint shader)                                          \
    X(GLuint,   glCreateShader,           GLenum type)                                            \
    X(void,     glGetIntegerv, GLenum pname, GLint *params )\
    X(void,     glGetProgramInfoLog,      GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) \
    X(void,     glGetProgramiv,           GLuint program, GLenum pname, GLint* params)            \
    X(void,     glGetShaderInfoLog,       GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source) \
    X(void,     glGetShaderiv,            GLuint shader, GLenum pname, GLint* params)             \
    X(void,     glLinkProgram,            GLuint program)                                         \
    X(void,     glShaderSource,           GLuint shader, GLsizei count, const char*string[], GLint *length) \
    X(void,     glTexImage2D,             GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) \
    X(void,     glTexParameteri,          GLenum target, GLenum pname, GLint param) \
    X(void,     glUniform1f,              GLint location, GLfloat v0)                             \
    X(void,     glUniform1i,              GLint location, GLint v0)                               \
    X(void,     glUniform2f,              GLint location, GLfloat v0, GLfloat v1)                 \
    X(void,     glUniform2fv,             GLint location, GLsizei count, GLfloat *value )   \
    X(void,     glUniform2i,              GLint location, GLint v0, GLint v1)                     \
    X(void,     glUniform2iv,             GLint location, GLsizei count, GLint *value )     \
    X(void,     glUniform3fv,             GLint location, GLsizei count, GLfloat *value )   \
    X(void,     glUniform3iv,             GLint location, GLsizei count, GLint *value )     \
    X(void,     glUniform4fv,             GLint location, GLsizei count, GLfloat *value )   \
    X(void,     glUniformMatrix2fv,       GLint location, GLsizei count, GLboolean tranpose, GLfloat* values) \
    X(void,     glUseProgram,             GLuint program)                                         \
    X(void,     glValidateProgram,        GLuint program)                                         \
    X(GLenum,   glCheckFramebufferStatusEXT, GLenum target)                                       \
    X(GLuint,   glCreateProgram,          void)                                                   \
    X(void,     glBindBuffer,             GLenum target, GLuint buffer)                           \
    X(void,     glBindVertexArray,        GLuint array)                                           \
    X(void,     glGenBuffers,             GLsizei n, GLuint *buffers)                             \
    X(void,     glGenVertexArrays,        GLsizei n, GLuint* arrays)                              \
    X(void,     glGetFloatv,              GLenum pname, GLfloat *data) \
    X(GLboolean,glIsEnabled,              GLenum cap)\
    X(void,     glActiveTexture,          GLenum texture)                                         \
    X(void,     glBlendEquation,          GLenum mode)                                            \
    X(void,     glBlendEquationSeparate,  GLenum modeRGB, GLenum modeAlpha)                       \
    X(void,     glBlendFunc,              GLenum source, GLenum dest) \
    X(void,     glBlitFramebufferEXT,     GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) \
    X(void,     glClear,                  GLbitfield mask) \
    X(void,     glClearColor, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)\
    X(void,     glClearDepth,             GLclampd depth) \
    X(void,     glCopyTexImage2D,         GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)\
    X(void,     glDeleteBuffers,          GLsizei n, GLuint* buffers)                       \
    X(void,     glDeleteVertexArrays,     GLsizei n, GLuint* arrays)                        \
    X(void,     glDepthFunc,              GLenum func) \
    X(void,     glDisable,                GLenum cap) \
    X(void,     glDrawArrays, GLenum mode, GLint first, GLsizei count)\
    X(void,     glDrawElements,           GLenum mode, GLsizei count, GLenum type, const void *indices)\
    X(void,     glEnableVertexAttribArray, GLuint index)                                          \
    X(void,     glPixelStorei,            GLenum pname, GLint param)\
    X(void,     glReadPixels,             GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)\
    X(void,     glScissor,                GLint x, GLint y, GLsizei width, GLsizei height) \
    X(void,     glUniformMatrix4fv,       GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) \
    X(void,     glVertexAttribPointer,    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLvoid *pointer) \
    X(void,     glViewport,               GLint x, GLint y, GLsizei width, GLsizei height)\
    X(void,     glDetachShader,           GLuint program, GLuint shader)                          \
    X(void,     glDeleteProgram,          GLuint program)                                         \
    X(void,     glDeleteTextures,         GLsizei n, const GLuint *textures)\
    X(void,     glDeleteShader,           GLuint shader)                                          \
    X(void, glPolygonMode,  GLenum face, GLenum mode) \

    // X(void,     glBindAttribLocation,     GLuint program, GLuint index, GLchar* name)       \
    // X(void,     glDeleteFramebuffersEXT,  GLsizei n, GLuint *framebuffers)                  \
    // X(void,     glDisableVertexAttribArray, GLuint index)                                         \
    // X(void,     glEnableClientState, GLenum array)\
    // X(void,     glTexImage2DMultisample,  GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) \
    // X(void,     glUniform3f,              GLint location, GLfloat v0, GLfloat v1, GLfloat v3)     \
    // X(void,     glUniform3i,              GLint location, GLint v0, GLint v1, GLint v3)           \
    // X(void,     glUniform4iv,             GLint location, GLsizei count, GLint *value )     \
    // X(void,     glUniformMatrix3fv,       GLint location, GLsizei count, GLboolean transpose, GLfloat* value) \
    //
    //
    //
    //
    //

#define GL_FUNCTIONS \
    GL_FUNCTIONS_GRAPHICS_DEBUG \
    GL_FUNCTIONS_DEBUG \
    GL_FUNCTIONS_CORE
