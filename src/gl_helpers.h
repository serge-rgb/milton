#pragma once

#define GLCHK(stmt) stmt; gl_query_error(#stmt, __FILE__, __LINE__)

void gl_query_error(const char* expr, const char* file, int line)
{
    GLenum err = glGetError();
    const char* str = "";
    if (err != GL_NO_ERROR)
    {
        char buffer[256];
        switch(err)
        {
        case GL_INVALID_ENUM:
            str = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            str = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            str = "GL_INVALID_OPERATION";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            str = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            str = "GL_OUT_OF_MEMORY";
            break;
        case GL_STACK_OVERFLOW:
            str = "GL_STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            str = "GL_STACK_UNDERFLOW";
            break;
        default:
            str = "SOME GL ERROR";
        }
        sprintf_s(buffer, 256, "%s in: %s:%d\n", str, file, line);
        OutputDebugStringA(buffer);
        sprintf_s(buffer, 256, "   ---- Expression: %s\n", expr);
        OutputDebugStringA(buffer);
    }
}

static GLuint gl_compile_shader(const char* src, GLuint type)
{
    GLuint obj = glCreateShader(type);
    GLCHK ( glShaderSource(obj, 1, &src, NULL) );
    GLCHK ( glCompileShader(obj) );
    // ERROR CHECKING
    int res = 0;
    GLCHK ( glGetShaderiv(obj, GL_COMPILE_STATUS, &res) );
    if (!res)
    {
        GLint length;
        GLCHK ( glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length) );
        char* log = (char*)malloc((size_t)length);
        GLsizei written_len;
        GLCHK ( glGetShaderInfoLog(obj, length, &written_len, log) );
        OutputDebugStringA("Shader compilation failed. \n    ---- Info log:\n");
        OutputDebugStringA(log);
        free(log);
    }
    return obj;
}

static void gl_link_program(GLuint obj, GLuint shaders[], int64_t num_shaders)
{
    assert(glIsProgram(obj));
    for (int i = 0; i < num_shaders; ++i)
    {
        assert(glIsShader(shaders[i]));
        GLCHK ( glAttachShader(obj, shaders[i]) );
    }
    GLCHK ( glLinkProgram(obj) );

    // ERROR CHECKING
    int res = 0;
    GLCHK ( glGetProgramiv(obj, GL_LINK_STATUS, &res) );
    if (!res)
    {
        OutputDebugStringA("ERROR: program did not link.\n");
        GLint len;
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
        GLsizei written_len;
        char* log = (char*)malloc((size_t)len);
        glGetProgramInfoLog(obj, (GLsizei)len, &written_len, log);
        OutputDebugStringA(log);
        free(log);
    }
    GLCHK ( glValidateProgram(obj) );
}

