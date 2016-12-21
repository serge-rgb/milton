// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


static int g_gl_helper_flags;

enum GLHelperFlags
{
    GLHelperFlags_SAMPLE_SHADING        = 1<<0,
    GLHelperFlags_TEXTURE_MULTISAMPLE   = 1<<1,
};

void
gl_helper_set_flags(int flags)
{
    g_gl_helper_flags |= flags;
}

bool
gl_helper_check_flags(int flags)
{
    bool result = g_gl_helper_flags & flags;
    return result;
}

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
    /*EXT_framebuffer_object*/\
    X(void,     glGenFramebuffersEXT,     GLsizei n, GLuint* framebuffers)                        \
    X(void,     glBindFramebufferEXT,     GLenum target, GLuint framebuffer)                      \
    X(GLenum,   glCheckFramebufferStatusEXT, GLenum target)                                       \
    X(void,     glDeleteFramebuffersEXT,  GLsizei n, const GLuint *framebuffers)                  \
    X(void,     glFramebufferTexture2DEXT, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) \
    /*EXT_framebuffer_blit*/\
    X(void,     glBlitFramebufferEXT,     GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) \
    /*ARB_texture_multisample*/\
    X(void,     glTexImage2DMultisample,  GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) \
    /* glMinSampleShadingARB gets treated separately because Milton can handle it missing. */ \
    /*X(void,     glMinSampleShadingARB,    GLclampf value)*/


#if defined(_WIN32)
// Function prototypes.
    #define X(ret, name, ...) typedef ret WINAPI name##Proc(__VA_ARGS__); name##Proc * name##;
        GL_FUNCTIONS
    #undef X

    // Declaring glMinSampleShadingARB because we have a different path for loading it.
    typedef void glMinSampleShadingARBProc(GLclampf value); glMinSampleShadingARBProc* glMinSampleShadingARB;
#endif  //_WIN32

bool
gl_load()
{
#if defined(_WIN32)
#define GETADDRESS(func, fatal_on_fail) \
                        { \
                            func = (decltype(func))wglGetProcAddress(#func); \
                            if ( func == NULL && fatal_on_fail )  { \
                                char* msg = "Could not load function " #func; \
                                milton_log(msg);\
                                milton_die_gracefully(msg); \
                            } \
                        }
#elif defined(__linux__)
    #define GETADDRESS(f, e)
    // There is no function prototype for the EXT version of this function but there is for GL core one.
    #define glBlitFramebufferEXT glBlitFramebuffer

    #if 0
    #define GETADDRESS(func) \
                        { \
                            milton_log("Loading OpenGL function " #func "\n"); \
                            func = (func##Proc))dlsym(lib, #func); \
                            if ( !func ) { \
                                milton_log("Could not load function " #func "\n"); \
                                milton_die_gracefully("Error loading GL function."); \
                            }\
                        }


    void* lib = dlopen("libGL.so", RTLD_LAZY);
    if (!lib) {
        printf("ERROR: libGL.so couldn't be loaded\n");
        return false;
    }
    #endif
#elif defined(__MACH__)
    #define GETADDRESS(f, e)

    // ARB_vertex_array_object on macOS not available, but apple extension is.
    #define glGenVertexArrays glGenVertexArraysAPPLE
    #define glBindVertexArray glBindVertexArrayAPPLE

#endif

    // Load
#define X(ret, name, ...) GETADDRESS(name, /*fatal on fail*/true)
    GL_FUNCTIONS
#undef X
    GETADDRESS(glMinSampleShadingARB, /*Not fatal on fail*/false)

    bool ok = true;
    // Extension checking.
    i64 num_extensions = 0;
    GLCHK( glGetIntegerv(GL_NUM_EXTENSIONS, (GLint*)&num_extensions) );

    if ( num_extensions > 0 ) {
        for ( i64 extension_i = 0; extension_i < num_extensions; ++extension_i ) {
            char* extension_string = (char*)glGetStringi(GL_EXTENSIONS, (GLuint)extension_i);

            #if MULTISAMPLING_ENABLED
                if ( strcmp(extension_string, "GL_ARB_sample_shading") == 0 ) {
                    gl_helper_set_flags(GLHelperFlags_SAMPLE_SHADING);
                }
                if ( strcmp(extension_string, "GL_ARB_texture_multisample") == 0 ) {
                    gl_helper_set_flags(GLHelperFlags_TEXTURE_MULTISAMPLE);
                }
            #endif
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
                    #if MULTISAMPLING_ENABLED
                        if ( strcmp(ext, "GL_ARB_sample_shading") == 0 ) {
                            gl_helper_set_flags(GLHelperFlags_SAMPLE_SHADING);
                        }
                        if ( strcmp(ext, "GL_ARB_texture_multisample") == 0 ) {
                            gl_helper_set_flags(GLHelperFlags_TEXTURE_MULTISAMPLE);
                        }
                    #endif
                    begin = end+1;
                }
                else {
                    milton_log("WARNING: Extension too large (%d)\n", len);
                }
            }
         }
    }

#if defined(_WIN32)
#pragma warning(push, 0)
    if ( !gl_helper_check_flags(GLHelperFlags_SAMPLE_SHADING) ) {
        glMinSampleShadingARB = NULL;
    }
#pragma warning(pop)
#undef GETADDRESS
#endif
    return ok;
}

void
gl_log(char* str)
{
#ifdef _WIN32
    OutputDebugStringA(str);
#else
    fprintf(stderr, "%s", str);
#endif
}

GLuint
gl_compile_shader(const char* in_src, GLuint type, char* config)
{
    const char* sources[] = {
        #if USE_GL_3_2
            "#version 330 \n",
        #else
            "#version 120\n",
            (type == GL_VERTEX_SHADER) ? "#define in attribute \n#define out varying\n"
                                       : "#define in varying   \n#define out\n#define out_color gl_FragColor\n",
            "#define texture texture2D\n",
        #endif
        (gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE)) ? "#define HAS_TEXTURE_MULTISAMPLE 1\n"
                                                                   : "#define HAS_TEXTURE_MULTISAMPLE 0\n",

        config,
        in_src
    };

    GLuint obj = glCreateShader(type);

    GLCHK ( glShaderSource(obj, array_count(sources), sources, NULL) );
    GLCHK ( glCompileShader(obj) );
    // ERROR CHECKING
    int res = 0;
    //GLCHK ( glGetObjectParameteriv(obj, GL_COMPILE_STATUS, &res) );
    GLCHK ( glGetShaderiv(obj, GL_COMPILE_STATUS, &res) );

    GLint length;
    GLCHK ( glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length) );
    if ( !res && length > 0 ) {
        if ( !res ) {
            milton_log("SHADER SOURCE:\n%s\n", sources[2]);
        }
        char* log = (char*)mlt_calloc(1, (size_t)length);
        GLsizei written_len;
        // GLCHK ( glGetShaderInfoLog(obj, length, &written_len, log) );
        GLCHK ( glGetShaderInfoLog (obj, length, &written_len, log) );
        gl_log("Shader compilation info. \n    ---- Info log:\n");
        gl_log(log);

        if ( !res ) {
            milton_die_gracefully("Shader compilation error\n");
        }

        free(log);
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
gl_link_program(GLuint obj, GLuint shaders[], int64_t num_shaders)
{
    mlt_assert(glIsProgram (obj));
    for ( int i = 0; i < num_shaders; ++i ) {
        mlt_assert(glIsShader(shaders[i]));

        GLCHK ( glAttachShader(obj, shaders[i]) );
    }
    GLCHK ( glLinkProgram(obj) );

    // ERROR CHECKING
    int res = 0;
    GLCHK ( glGetProgramiv(obj, GL_LINK_STATUS, &res) );
    if ( !res ) {
        gl_log("ERROR: program did not link.\n");
        GLint len;
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
        GLsizei written_len;
        char* log = (char*)mlt_calloc(1, (size_t)len);
        glGetProgramInfoLog(obj, (GLsizei)len, &written_len, log);
        //glGetInfoLog(obj, (GLsizei)len, &written_len, log);
        gl_log(log);
        free(log);
        mlt_assert(!"program linking error");
    }
    GLCHK ( glValidateProgram(obj) );
}
#if defined(__MACH__)
#undef glGetObjectParameterivARB
#undef glGetInfoLogARB
#undef glAttachObjectARB
#undef glLinkProgramARB
#undef glValidateProgramARB
#undef glUseProgramObjectARB
#endif

void
gl_query_error(const char* expr, const char* file, int line)
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
        gl_log(buffer);
        snprintf(buffer, 256, "   ---- Expression: %s\n", expr);
        gl_log(buffer);
    }
}


bool
gl_set_attribute_vec2(GLuint program, char* name, GLfloat* data, size_t data_sz)
{
    bool ok = true;
    GLint loc = glGetAttribLocation(program, name);
    ok = loc >= 0;
    if ( ok ) {
        GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data_sz, data, GL_STATIC_DRAW) );
    }

    return ok;
}

bool
gl_set_uniform_vec4(GLuint program, char* name, size_t count, float* vals)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;

    if ( ok ) {
        GLCHK( glUniform4fv(loc, (GLsizei)count, vals) );
    }
    return ok;
}

bool
gl_set_uniform_vec3i(GLuint program, char* name, size_t count, i32* vals)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;

    if ( ok ) {
        GLCHK( glUniform3iv(loc, (GLsizei)count, vals) );
    }
    return ok;
}

bool
gl_set_uniform_vec3(GLuint program, char* name, size_t count, float* vals)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;

    if ( ok ) {
        GLCHK( glUniform3fv(loc, (GLsizei)count, vals) );
    }
    return ok;
}

bool
gl_set_uniform_vec2(GLuint program, char* name, size_t count, float* vals)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if ( ok ) {
        GLCHK( glUniform2fv(loc, (GLsizei)count, vals) );
    }
    return ok;
}

bool
gl_set_uniform_vec2(GLuint program, char* name, float x, float y)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if ( ok ) {
        GLCHK( glUniform2f(loc, x, y) );
    }
    return ok;
}

bool
gl_set_uniform_vec2i(GLuint program, char* name, size_t count, i32* vals)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if ( ok ) {
        GLCHK( glUniform2iv(loc, (GLsizei)count, vals) );
    }
    return ok;
}

bool
gl_set_uniform_f(GLuint program, char* name, float val)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if ( ok ) {
        GLCHK( glUniform1f(loc, val) );
    }
    return ok;
}

bool
gl_set_uniform_i(GLuint program, char* name, i32 val)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if ( ok ) {
        GLCHK( glUniform1i(loc, val) );
    }
    return ok;
}

bool
gl_set_uniform_vec2i(GLuint program, char* name, i32 x, i32 y)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if ( ok ) {
        GLCHK( glUniform2i(loc, x, y) );
    }
    return ok;
}

#ifdef _WIN32

#endif  // _WIN32

