// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


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

GLuint gl_compile_shader(const char* src, GLuint type)
{
    GLuint obj = glCreateShader(type);

    GLCHK ( glShaderSource(obj, 1, &src, NULL) );
    GLCHK ( glCompileShader(obj) );
    // ERROR CHECKING
    int res = 0;
    //GLCHK ( glGetObjectParameteriv(obj, GL_COMPILE_STATUS, &res) );
    GLCHK ( glGetShaderiv(obj, GL_COMPILE_STATUS, &res) );
    if (!res)
    {
		milton_log("SHADER SOURCE:\n%s\n", src);
        GLint length;
        GLCHK ( glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length) );
        char* log = (char*)malloc((size_t)length);
        GLsizei written_len;
        // GLCHK ( glGetShaderInfoLog(obj, length, &written_len, log) );
        GLCHK ( glGetShaderInfoLog (obj, length, &written_len, log) );
        gl_log("Shader compilation failed. \n    ---- Info log:\n");
        gl_log(log);
        free(log);
        
        mlt_assert(!"Shader compilation error");
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

void gl_link_program(GLuint obj, GLuint shaders[], int64_t num_shaders)
{
    mlt_assert(glIsProgram (obj));
    for (int i = 0; i < num_shaders; ++i)
    {
        mlt_assert(glIsShader(shaders[i]));

        GLCHK ( glAttachShader(obj, shaders[i]) );
    }
    GLCHK ( glLinkProgram(obj) );

    // ERROR CHECKING
    int res = 0;
    GLCHK ( glGetProgramiv(obj, GL_LINK_STATUS, &res) );
    if (!res)
    {
        gl_log("ERROR: program did not link.\n");
        GLint len;
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
        GLsizei written_len;
        char* log = (char*)malloc((size_t)len);
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

void gl_query_error(const char* expr, const char* file, int line)
{
    GLenum err = glGetError();
    const char* str = "";
    if (err != GL_NO_ERROR)
    {
        char buffer[256];
        switch(err)
        {
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


bool gl_set_attribute_vec2(GLuint program, char* name, GLfloat* data, size_t data_sz)
{
    bool ok = true;
    GLint loc = glGetAttribLocation(program, name);
    ok = loc >= 0;
    if (ok)
    {
        GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data_sz, data, GL_STATIC_DRAW) );
    }

    return ok;
}

bool gl_set_uniform_vec4(GLuint program, char* name, size_t count, float* vals)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;

    if (ok)
    {
        GLCHK( glUniform4fv(loc, (GLsizei)count, vals) );
    }
    return ok;
}

bool gl_set_uniform_vec3(GLuint program, char* name, size_t count, float* vals)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;

    if (ok)
    {
        GLCHK( glUniform3fv(loc, (GLsizei)count, vals) );
    }
    return ok;
}

bool gl_set_uniform_vec2(GLuint program, char* name, size_t count, float* vals)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if (ok)
    {
        GLCHK( glUniform2fv(loc, (GLsizei)count, vals) );
    }
    return ok;
}

bool gl_set_uniform_vec2(GLuint program, char* name, float x, float y)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if (ok)
    {
        GLCHK( glUniform2f(loc, x, y) );
    }
    return ok;
}

bool gl_set_uniform_vec2i(GLuint program, char* name, size_t count, i32* vals)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if (ok)
    {
        GLCHK( glUniform2iv(loc, (GLsizei)count, vals) );
    }
    return ok;
}

bool gl_set_uniform_f(GLuint program, char* name, float val)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if (ok)
    {
        GLCHK( glUniform1f(loc, val) );
    }
    return ok;
}

bool gl_set_uniform_i(GLuint program, char* name, i32 val)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if (ok)
    {
        GLCHK( glUniform1i(loc, val) );
    }
    return ok;
}

bool gl_set_uniform_vec2i(GLuint program, char* name, i32 x, i32 y)
{
    glUseProgram(program);
    bool ok = true;
    GLint loc = glGetUniformLocation(program, name);
    ok = loc >= 0;
    if (ok)
    {
        GLCHK( glUniform2i(loc, x, y) );
    }
    return ok;
}

#ifdef _WIN32

#endif  // _WIN32

