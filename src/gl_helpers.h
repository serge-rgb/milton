// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "common.h"
#include "system_includes.h"

#include "gl_functions.inl"

#if defined(_WIN32)
    // OpenGL function prototypes.
    #define X(ret, name, ...) typedef ret WINAPI name##Proc(__VA_ARGS__); extern name##Proc * name##;
        GL_FUNCTIONS
    #undef X

    // Declaring glMinSampleShadingARB because we have a different path for loading it.
    typedef void glMinSampleShadingARBProc(GLclampf value); extern glMinSampleShadingARBProc* glMinSampleShadingARB;
#endif  //_WIN32


enum GLHelperFlags
{
    GLHelperFlags_SAMPLE_SHADING        = 1<<0,
    GLHelperFlags_TEXTURE_MULTISAMPLE   = 1<<1,
};

bool gl_helper_check_flags(int flags);

#if MILTON_DEBUG
#define GLCHK(stmt) stmt; gl_query_error(#stmt, __FILE__, __LINE__)
#else
#define GLCHK(stmt) stmt;
#endif

bool    gl_load();
void    gl_log(char* str);
void    gl_query_error(const char* expr, const char* file, int line);
GLuint  gl_compile_shader(const char* src, GLuint type, char* config = "");
void    gl_link_program(GLuint obj, GLuint shaders[], int64_t num_shaders);

bool gl_set_attribute_vec2(GLuint program, char* name, GLfloat* data, size_t data_sz);
bool gl_set_uniform_vec4(GLuint program, char* name, size_t count, float* vals);
bool gl_set_uniform_vec4(GLuint program, char* name, size_t count, float* vals);
bool gl_set_uniform_vec3i(GLuint program, char* name, size_t count, i32* vals);
bool gl_set_uniform_vec3(GLuint program, char* name, size_t count, float* vals);
bool gl_set_uniform_vec2(GLuint program, char* name, size_t count, float* vals);
bool gl_set_uniform_vec2(GLuint program, char* name, float x, float y);
bool gl_set_uniform_vec2i(GLuint program, char* name, size_t count, i32* vals);
bool gl_set_uniform_f(GLuint program, char* name, float val);
bool gl_set_uniform_i(GLuint program, char* name, i32 val);
bool gl_set_uniform_vec2i(GLuint program, char* name, i32 x, i32 y);

GLuint gl_new_color_texture(int w, int h);
GLuint gl_new_color_texture_multisample(int w, int h);
GLuint gl_new_depth_stencil_texture(int w, int h);
GLuint gl_new_depth_stencil_texture_multisample(int w, int h);
GLuint gl_new_fbo(GLuint color_attachment, GLuint depth_stencil_attachment=0, GLenum texture_target=GL_TEXTURE_2D);

void gl_resize_color_texture(GLuint t, int w, int h);
void gl_resize_color_texture_multisample(GLuint t, int w, int h);
void gl_resize_depth_stencil_texture(GLuint t, int w, int h);
void gl_resize_depth_stencil_texture_multisample(GLuint t, int w, int h);



#if defined(__linux__)
    // There is no function prototype for the EXT version of this function but there is for GL core one.
    #define glBlitFramebufferEXT glBlitFramebuffer
#endif
