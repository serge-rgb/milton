// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "common.h"
#include "system_includes.h"

#include "gl.h"

// GL_DEBUG_OUTPUT defines
#define GL_DEBUG_OUTPUT                   0x92E0

#define GL_DEBUG_CALLBACK(name)  void name(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)


typedef GL_DEBUG_CALLBACK(GlDebugCallback);

GL_DEBUG_CALLBACK(milton_gl_debug_callback);

#if defined(_WIN32)
    // Declaring glMinSampleShadingARB because we have a different path for loading it.
    typedef void glMinSampleShadingARBProc (GLclampf value); extern glMinSampleShadingARBProc* glMinSampleShadingARB;
#endif  //_WIN32


enum GLHelperFlags
{
    GLHelperFlags_SAMPLE_SHADING        = 1<<0,
    GLHelperFlags_TEXTURE_MULTISAMPLE   = 1<<1,
};

namespace gl {

bool    check_flags (int flags);

bool    load ();
void    log (char* str);
GLuint  compile_shader (const char* src, GLuint type, char* config = "");
void    link_program (GLuint obj, GLuint shaders[], int64_t num_shaders);

bool    set_attribute_vec2 (GLuint program, char* name, GLfloat* data, size_t data_sz);
bool    set_uniform_vec4 (GLuint program, char* name, size_t count, float* vals);
bool    set_uniform_vec4 (GLuint program, char* name, size_t count, float* vals);
bool    set_uniform_vec3i (GLuint program, char* name, size_t count, i32* vals);
bool    set_uniform_vec3 (GLuint program, char* name, size_t count, float* vals);
bool    set_uniform_vec2 (GLuint program, char* name, size_t count, float* vals);
bool    set_uniform_vec2 (GLuint program, char* name, float x, float y);
bool    set_uniform_vec2i (GLuint program, char* name, size_t count, i32* vals);
bool    set_uniform_f (GLuint program, char* name, float val);
bool    set_uniform_i (GLuint program, char* name, i32 val);
bool    set_uniform_vec2i (GLuint program, char* name, i32 x, i32 y);

GLuint  new_color_texture (int w, int h);
GLuint  new_depth_stencil_texture (int w, int h);
GLuint  new_fbo (GLuint color_attachment, GLuint depth_stencil_attachment=0, GLenum texture_target=GL_TEXTURE_2D);

#if MULTISAMPLING_ENABLED
GLuint  new_color_texture_multisample (int w, int h);
GLuint  new_depth_stencil_texture_multisample (int w, int h);
void    resize_color_texture_multisample (GLuint t, int w, int h);
void    resize_depth_stencil_texture_multisample (GLuint t, int w, int h);
#endif

void    resize_color_texture (GLuint t, int w, int h);
void    resize_depth_stencil_texture (GLuint t, int w, int h);

}  // namespace gl

#if defined(__linux__)
    // There is no function prototype for the EXT version of this function but there is for GL core one.
    #define glBlitFramebufferEXT glBlitFramebuffer
#endif
