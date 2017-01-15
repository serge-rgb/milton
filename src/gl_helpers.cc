// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "gl_helpers.h"

#include "memory.h"
#include "platform.h"

#include "gl_functions.inl"

#if defined(_WIN32)
    // OpenGL function prototypes.
    #define X(ret, name, ...) typedef ret WINAPI name##Proc(__VA_ARGS__); name##Proc * name##;
        GL_FUNCTIONS
    #undef X

    // Declaring glMinSampleShadingARB because we have a different path for loading it.
    typedef void glMinSampleShadingARBProc(GLclampf value); glMinSampleShadingARBProc* glMinSampleShadingARB;
#endif  //_WIN32

static int g_gl_helper_flags;

static void
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
        char* log = (char*)mlt_calloc(1, (size_t)length, "Strings");
        GLsizei written_len;
        // GLCHK ( glGetShaderInfoLog(obj, length, &written_len, log) );
        GLCHK ( glGetShaderInfoLog (obj, length, &written_len, log) );
        gl_log("Shader compilation info. \n    ---- Info log:\n");
        gl_log(log);

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
        char* log = (char*)mlt_calloc(1, (size_t)len, "Strings");
        glGetProgramInfoLog(obj, (GLsizei)len, &written_len, log);
        //glGetInfoLog(obj, (GLsizei)len, &written_len, log);
        gl_log(log);
        mlt_free(log, "Strings");
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

