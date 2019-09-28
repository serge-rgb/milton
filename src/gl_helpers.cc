// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "gl_helpers.h"
#include "gl.h"

#include "memory.h"
#include "platform.h"



#if defined(_WIN32)
    // Declaring glMinSampleShadingARB because we have a different path for loading it.
    typedef void glMinSampleShadingARBProc(GLclampf value); glMinSampleShadingARBProc* glMinSampleShadingARB;
#endif  //_WIN32


// Global variable that keeps track of Milton's GL configuration. See GLHelperFlags.
static int g_gl_helper_flags;

namespace gl {

// Static helpers
static void
query_error (const char* expr, const char* file, int line)
{
    GLenum err = glGetError();
    const char* str = "";
    if ( err != GL_NO_ERROR ) {
        char buffer[256];
        switch( err ) {
#ifdef GL_INVALID_ENUM
        case GL_INVALID_ENUM:
            str = "GL_INVALID_ENUM";
            break;
#endif
#ifdef GL_INVALID_VALUE
        case GL_INVALID_VALUE:
            str = "GL_INVALID_VALUE";
            break;
#endif
#ifdef GL_INVALID_OPERATION
        case GL_INVALID_OPERATION:
            str = "GL_INVALID_OPERATION";
            break;
#endif
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            str = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
#endif
#ifdef GL_OUT_OF_MEMORY
        case GL_OUT_OF_MEMORY:
            str = "GL_OUT_OF_MEMORY";
            break;
#endif
#ifdef GL_STACK_OVERFLOW
        case GL_STACK_OVERFLOW:
            str = "GL_STACK_OVERFLOW";
            break;
#endif
#ifdef GL_STACK_UNDERFLOW
        case GL_STACK_UNDERFLOW:
            str = "GL_STACK_UNDERFLOW";
            break;
#endif
        default:
            str = "SOME GL ERROR";
            break;
        }
        snprintf(buffer, 256, "%s in: %s:%d\n", str, file, line);
        gl::log(buffer);
        snprintf(buffer, 256, "   ---- Expression: %s\n", expr);
        gl::log(buffer);
    }
}

static void
set_flags (int flags)
{
    g_gl_helper_flags |= flags;
}

bool
check_flags (int flags)
{
    bool result = g_gl_helper_flags & flags;
    return result;
}

bool
load ()
{
#define X(ret, func, ...) func = (decltype(func)) platform_get_gl_proc(#func);
    GL_FUNCTIONS
#undef X

    bool ok = true;
    // Extension checking.

#if MULTISAMPLING_ENABLED
    i64 num_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, (GLint*)&num_extensions);

    if ( num_extensions > 0 ) {
        for ( i64 extension_i = 0; extension_i < num_extensions; ++extension_i ) {
            char* extension_string = (char*)glGetStringi(GL_EXTENSIONS, (GLuint)extension_i);

                if ( strcmp(extension_string, "GL_ARB_sample_shading") == 0 ) {
                    gl::set_flags(GLHelperFlags_SAMPLE_SHADING);
                }
                if ( strcmp(extension_string, "GL_ARB_texture_multisample") == 0 ) {
                    gl::set_flags(GLHelperFlags_TEXTURE_MULTISAMPLE);
                }
        }
    }
    // glGetStringi probably does not handle GL_EXTENSIONS
    else if ( num_extensions == 0 ) {
        const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
        #define MAX_EXTENSION_LEN 256
        char ext[MAX_EXTENSION_LEN] = {};
        const char* begin = extensions;
        for ( const char* end = extensions;
              *end != '\0';
              ++end ) {
            if ( *end == ' ' ) {
                size_t len = (size_t)end - (size_t)begin;

                if ( len < MAX_EXTENSION_LEN ) {
                    memcpy((void*)ext, (void*)begin, len);
                    ext[len]='\0';
                        if ( strcmp(ext, "GL_ARB_sample_shading") == 0 ) {
                            gl::set_flags(GLHelperFlags_SAMPLE_SHADING);
                        }
                        if ( strcmp(ext, "GL_ARB_texture_multisample") == 0 ) {
                            gl::set_flags(GLHelperFlags_TEXTURE_MULTISAMPLE);
                        }
                    begin = end+1;
                }
                else {
                    milton_log("WARNING: Extension too large (%d)\n", len);
                }
            }
        }
    }
#endif

#if defined(_WIN32)
#pragma warning(push, 0)
    if ( !check_flags(GLHelperFlags_SAMPLE_SHADING) ) {
        glMinSampleShadingARB = NULL;
    }
#pragma warning(pop)
#undef GETADDRESS
#endif
    return ok;
}

void
log (char* str)
{
#ifdef _WIN32
    OutputDebugStringA(str);
#else
    fprintf(stderr, "%s", str);
#endif
}

GLuint
compile_shader (const char* in_src, GLuint type, char* config, char* variation_config)
{
    const char* sources[] = {
        #if USE_GL_3_2
            "#version 330 \n",
        #else
            "#version 120\n",
            //"#extension GL_ARB_gpu_shader5 : disable \n",
            // "#extension GL_ARB_gpu_shader4 : enable \n",
            (type == GL_VERTEX_SHADER) ? "#define in attribute \n#define out varying\n"
                                       : "#define in varying   \n#define out\n#define out_color gl_FragColor\n",
            "#define texture texture2D\n",
        #endif
        #if STROKE_DEBUG_VIZ
            "#define STROKE_DEBUG_VIZ 1\n",
        #else
            "#define STROKE_DEBUG_VIZ 0\n",
        #endif
        (check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE)) ? "#define HAS_TEXTURE_MULTISAMPLE 1\n"
                                                                    : "#define HAS_TEXTURE_MULTISAMPLE 0\n",
        "#if HAS_TEXTURE_MULTISAMPLE\n",
        "#extension GL_ARB_sample_shading : enable\n",
        //" #extension GL_ARB_texture_multisample : enable\n",
        "#endif\n",
#if USE_GL_3_2
        (type == GL_FRAGMENT_SHADER) ? "out vec4 out_color; \n" : "\n",
#endif

        config,
        variation_config,
        in_src
    };

    GLuint obj = glCreateShader(type);

    glShaderSource(obj, array_count(sources), sources, NULL);
    glCompileShader(obj);
    // ERROR CHECKING
    int res = 0;
    //glGetObjectParameteriv(obj, GL_COMPILE_STATUS, &res);
    glGetShaderiv(obj, GL_COMPILE_STATUS, &res);

    GLint length;
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);
    if ( !res && length > 0 ) {
        if ( !res ) {
            milton_log("SHADER SOURCE:\n%s\n", sources[2]);
        }
        char* log = (char*)mlt_calloc(1, (size_t)length, "Strings");
        GLsizei written_len;
        // glGetShaderInfoLog(obj, length, &written_len, log);
        glGetShaderInfoLog (obj, length, &written_len, (GLchar*)log);
        gl::log("Shader compilation info. \n    ---- Info log:\n");
        gl::log(log);

        if ( !res ) {
            milton_die_gracefully("Shader compilation error\n");
        }

        mlt_free(log, "Strings");
    }
    return obj;
}

#if defined(__MACH__)
#undef glShaderSourceARB
#undef glCompileShaderARB
#undef glGetObjectParameterivARB
#undef glGetInfoLogARB
#define glGetObjectParameterivARB glGetProgramiv
#define glGetInfoLogARB glGetProgramInfoLog
#define glAttachObjectARB glAttachShader
#define glLinkProgramARB glLinkProgram
#define glValidateProgramARB glValidateProgram
#define glUseProgramObjectARB glUseProgram
#endif

void
link_program (GLuint obj, GLuint shaders[], int64_t num_shaders)
{
    mlt_assert(glIsProgram (obj));
    for ( int i = 0; i < num_shaders; ++i ) {
        mlt_assert(glIsShader(shaders[i]));

        glAttachShader(obj, shaders[i]);
    }
    glLinkProgram(obj);

    // ERROR CHECKING
    int res = 0;
    glGetProgramiv(obj, GL_LINK_STATUS, &res);
    if ( !res ) {
        gl::log("ERROR: program did not link.\n");
        GLint len;
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
        GLsizei written_len;
        char* log = (char*)mlt_calloc(1, (size_t)len, "Strings");
        glGetProgramInfoLog(obj, (GLsizei)len, &written_len, (GLchar*)log);
        //glGetInfoLog(obj, (GLsizei)len, &written_len, log);
        gl::log(log);
        mlt_free(log, "Strings");
        mlt_assert(!"program linking error");
    }
    glValidateProgram(obj);
}
#if defined(__MACH__)
#undef glGetObjectParameterivARB
#undef glGetInfoLogARB
#undef glAttachObjectARB
#undef glLinkProgramARB
#undef glValidateProgramARB
#undef glUseProgramObjectARB
#endif

bool
set_attribute_vec2(GLuint program, char* name, GLfloat* data, size_t data_sz)
{
    bool ok = true;
    GLint loc = glGetAttribLocation(program, (GLchar*)name);
    ok = loc >= 0;
    if ( ok ) {
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data_sz, data, GL_STATIC_DRAW);
    }

    return ok;
}

void
use_program(GLuint program)
{
    static GLuint cached_program = 0;
    if (program != cached_program) {
        glUseProgram(program);
        cached_program = program;
    }
}

bool
set_uniform_vec4(GLuint program, char* name, size_t count, float* vals)
{
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    use_program(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, (GLchar*)name);
    ok = loc >= 0;

    if ( ok ) {
        glUniform4fv(loc, (GLsizei)count, vals);
    }
    use_program(last_program);
    return ok;
}

bool
set_uniform_vec3i(GLuint program, char* name, size_t count, i32* vals)
{
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    use_program(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, (GLchar*)name);
    ok = loc >= 0;

    if ( ok ) {
        glUniform3iv(loc, (GLsizei)count, vals);
    }
    use_program(last_program);
    return ok;
}

bool
set_uniform_vec3(GLuint program, char* name, size_t count, float* vals)
{
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    use_program(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, (GLchar*)name);
    ok = loc >= 0;

    if ( ok ) {
        glUniform3fv(loc, (GLsizei)count, vals);
    }
    use_program(last_program);
    return ok;
}

bool
set_uniform_vec2(GLuint program, char* name, size_t count, float* vals)
{
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    use_program(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, (GLchar*)name);
    ok = loc >= 0;
    if ( ok ) {
        glUniform2fv(loc, (GLsizei)count, vals);
    }
    use_program(last_program);
    return ok;
}

bool
set_uniform_vec2(GLuint program, char* name, float x, float y)
{
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    use_program(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, (GLchar*)name);
    ok = loc >= 0;
    if ( ok ) {
        glUniform2f(loc, x, y);
    }
    use_program(last_program);
    return ok;
}

bool
set_uniform_vec2i(GLuint program, char* name, size_t count, i32* vals)
{
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    use_program(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, (GLchar*)name);
    ok = loc >= 0;
    if ( ok ) {
        glUniform2iv(loc, (GLsizei)count, vals);
    }
    use_program(last_program);
    return ok;
}

bool
set_uniform_f(GLuint program, char* name, float val)
{
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    use_program(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, (GLchar*)name);
    ok = loc >= 0;
    if ( ok ) {
        glUniform1f(loc, val);
    }
    use_program(last_program);
    return ok;
}

bool
set_uniform_i(GLuint program, char* name, i32 val)
{
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    use_program(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, (GLchar*)name);
    ok = loc >= 0;
    if ( ok ) {
        glUniform1i(loc, val);
    }
    use_program(last_program);
    return ok;
}

bool
set_uniform_vec2i(GLuint program, char* name, i32 x, i32 y)
{
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    use_program(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, (GLchar*)name);
    ok = loc >= 0;
    if ( ok ) {
        glUniform2i(loc, x, y);
    }
    use_program(last_program);
    return ok;
}

bool
set_uniform_mat2 (GLuint program, char* name, f32* vals)
{
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    use_program(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, (GLchar*)name);
    ok = loc >= 0;
    if ( ok ) {
        glUniformMatrix2fv(loc, 1, /*transpose*/false, vals);
    }
    use_program(last_program);
    return ok;
}


GLuint
new_color_texture(int w, int h)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_RGBA8,
                 /*width, height = */ w, h,
                 /*border = */ 0,
                 /*format = */ GL_RGBA, /*type = */ GL_FLOAT,
                 /*data = */ NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

GLuint
new_depth_stencil_texture(int w, int h)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_DEPTH24_STENCIL8,
                 /*width, height = */ w,h,
                 /*border = */ 0,
                 /*format = */ GL_DEPTH_STENCIL, /*type = */ GL_UNSIGNED_INT_24_8,
                 /*data = */ NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

GLuint
new_fbo(GLuint color_attachment, GLuint depth_stencil_attachment, GLenum texture_target)
{
    GLuint fbo = 0;
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);


    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                              color_attachment, 0);
    if ( depth_stencil_attachment ) {
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, texture_target,
                                  depth_stencil_attachment, 0);
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
    return fbo;
}


GLuint
new_color_texture_multisample(int w, int h)
{
#if MULTISAMPLING_ENABLED
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, t);

    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                            GL_RGBA,
                            w,h,
                            GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    return t;
#else
    return 0;
#endif
}

GLuint
new_depth_stencil_texture_multisample(int w, int h)
{
#if MULTISAMPLING_ENABLED
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, t);

    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                            /*internalFormat, num of components*/GL_DEPTH24_STENCIL8,
                            w,h,
                            GL_TRUE);


    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    return t;
#else
    return 0;
#endif
}

void
resize_color_texture_multisample(GLuint t, int w, int h)
{
#if MULTISAMPLING_ENABLED
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, t);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                            GL_RGBA,
                            w,h,
                            GL_TRUE);
#endif
}

void
resize_depth_stencil_texture_multisample(GLuint t, int w, int h)
{
#if MULTISAMPLING_ENABLED
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, t);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                            GL_DEPTH24_STENCIL8,
                            w,h,
                            GL_TRUE);

#endif
}

void
resize_color_texture(GLuint t, int w, int h)
{
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_RGBA8,
                 /*width, height = */ w,h,
                 /*border = */ 0,
                 /*format = */ GL_RGBA, /*type = */ GL_UNSIGNED_BYTE,
                 /*data = */ NULL);
}


void
resize_depth_stencil_texture(GLuint t, int w, int h)
{
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_DEPTH24_STENCIL8,
                 /*width, height = */ w,h,
                 /*border = */ 0,
                 /*format = */ GL_DEPTH_STENCIL, /*type = */ GL_UNSIGNED_INT_24_8,
                 /*data = */ NULL);
}

void
vertex_attrib_v3f(GLuint program, char* name, GLuint vbo)
{
    GLint loc = glGetAttribLocation(program, name);
    if (loc >= 0) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray((GLuint)loc);
        glVertexAttribPointer(/*attrib location*/ (GLuint)loc,
                              /*size*/ 3, GL_FLOAT, /*normalize*/ GL_FALSE,
                              /*stride*/ 0, /*ptr*/ 0);
    }
}

void
vertex_attrib_v2f(GLuint program, char* name, GLuint vbo)
{
    GLint loc = glGetAttribLocation(program, name);
    if (loc >= 0) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray((GLuint)loc);
        glVertexAttribPointer(/*attrib location*/ (GLuint)loc,
                              /*size*/ 2, GL_FLOAT, /*normalize*/ GL_FALSE,
                              /*stride*/ 0, /*ptr*/ 0);
    }
}


}  // namespace gl

#ifdef _WIN32

#endif  // _WIN32

