// gl_helpers.h
//  Sergio Gonzalez
//
// (2015-09-15) - Updated to depend on GL_ARB_shader_objects,
//  ARB_vertex_program, and ARB_fragment_program to increase compatibility
//
//
// This software is in the public domain. Where that dedication is not
// recognized, you are granted a perpetual, irrevocable license to copy
// and modify this file as you see fit.

#pragma once

#define GLCHK(stmt) stmt; gl_query_error(#stmt, __FILE__, __LINE__)


void gl_log(char* str);
void gl_query_error(const char* expr, const char* file, int line);
static GLuint gl_compile_shader(const char* src, GLuint type);
static void gl_link_program(GLuint obj, GLuint shaders[], int64_t num_shaders);

#ifdef _WIN32 // sample OpenGL creation func
static int sgl_win32_setup_context(HWND window, HGLRC* context);
#endif

#ifdef SGL_GL_HELPERS_IMPLEMENTATION

void gl_log(char* str)
{
#ifdef _WIN32
    OutputDebugStringA(str);
#else
    fprintf(stderr, "%s", str);
#endif
}


// Apple defines GLhandleARB as void*
// our simple solution is to define functions as their core counterparts, which should be
// very likely to be present in a random Mac system
#if defined(__MACH__)
#define glCreateShaderObjectARB glCreateShader
#define glShaderSourceARB glShaderSource
#define glCompileShaderARB glCompileShader
#define glGetObjectParameterivARB glGetShaderiv
#define glGetInfoLogARB glGetShaderInfoLog

#endif

static GLuint gl_compile_shader(const char* src, GLuint type)
{
    GLuint obj = (GLuint)glCreateShaderObjectARB(type);

    GLCHK ( glShaderSourceARB(obj, 1, &src, NULL) );
    GLCHK ( glCompileShaderARB(obj) );
    // ERROR CHECKING
    int res = 0;
    GLCHK ( glGetObjectParameterivARB(obj, GL_OBJECT_COMPILE_STATUS_ARB, &res) );
    if (!res)
    {
        GLint length;
        // GLCHK ( glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length) );
        GLCHK ( glGetObjectParameterivARB(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length) );
        char* log = (char*)malloc((size_t)length);
        GLsizei written_len;
        // GLCHK ( glGetShaderInfoLog(obj, length, &written_len, log) );
        GLCHK ( glGetInfoLogARB(obj, length, &written_len, log) );
        gl_log("Shader compilation failed. \n    ---- Info log:\n");
        gl_log(log);
        free(log);
        assert(!"Shader compilation error");
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

static void gl_link_program(GLuint obj, GLuint shaders[], int64_t num_shaders)
{
    //assert(glIsProgramARB(obj));
    for (int i = 0; i < num_shaders; ++i)
    {
        // assert(glIsShader(shaders[i]));
        // TODO: Is there an equivalent to glIsShader?

        // GLCHK ( glAttachShader(obj, shaders[i]) );
        GLCHK ( glAttachObjectARB(obj, shaders[i]) );

    }
    GLCHK ( glLinkProgramARB(obj) );

    // ERROR CHECKING
    int res = 0;
    //GLCHK ( glGetProgramiv(obj, GL_LINK_STATUS, &res) );
    GLCHK ( glGetObjectParameterivARB(obj, GL_OBJECT_LINK_STATUS_ARB, &res) );
    if (!res) {
        gl_log("ERROR: program did not link.\n");
        GLint len;
        // glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
        glGetObjectParameterivARB(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
        GLsizei written_len;
        char* log = (char*)malloc((size_t)len);
        //glGetProgramInfoLog(obj, (GLsizei)len, &written_len, log);
        glGetInfoLogARB(obj, (GLsizei)len, &written_len, log);
        gl_log(log);
        free(log);
        assert(!"program linking error");
    }
    GLCHK ( glValidateProgramARB(obj) );
}
#if defined(__MACH__)
#undef glGetObjectParameterivARB
#undef glGetInfoLogARB
#undef glAttachObjectARB
#undef glLinkProgramARB
#undef glValidateProgramARB
#undef glUseProgramObjectARB
#endif

void gl_query_error(const char* expr, const char* file, int line)
{
    GLenum err = glGetError();
    const char* str = "";
    if (err != GL_NO_ERROR) {
        char buffer[256];
        switch(err) {
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

#ifdef _WIN32

#endif  // _WIN32
#endif  // SGL_GL_HELPERS_IMPLEMENTATION
