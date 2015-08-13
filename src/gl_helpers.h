// gl_helpers.h
// (c) Copyright 2015 Sergio Gonzalez
//
// Released under the MIT license. See LICENSE.txt

#pragma once

#define GLCHK(stmt) stmt; gl_query_error(#stmt, __FILE__, __LINE__)

void gl_log(char* str)
{
#ifdef _WIN32
    OutputDebugStringA(str);
#else
    fprintf(stderr, "%s", str);
#endif
}

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
        }
        snprintf(buffer, 256, "%s in: %s:%d\n", str, file, line);
        gl_log(buffer);
        snprintf(buffer, 256, "   ---- Expression: %s\n", expr);
        gl_log(buffer);
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
        gl_log("Shader compilation failed. \n    ---- Info log:\n");
        gl_log(log);
        free(log);
        assert(!"Shader compilation error");
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
        gl_log("ERROR: program did not link.\n");
        GLint len;
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
        GLsizei written_len;
        char* log = (char*)malloc((size_t)len);
        glGetProgramInfoLog(obj, (GLsizei)len, &written_len, log);
        gl_log(log);
        free(log);
        assert(!"program linking error");
    }
    GLCHK ( glValidateProgram(obj) );
}

#ifdef _WIN32
// Will setup an OpenGL 3.3 core profile context.
// Loads functions with GLEW
static int win32_setup_context(HWND window, HGLRC* context)
{
    int format_index = 0;

    PIXELFORMATDESCRIPTOR pfd = { 0 };
    {
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags =
            PFD_DRAW_TO_WINDOW |   // support window
            PFD_SUPPORT_OPENGL |   // support OpenGL
            PFD_DOUBLEBUFFER;      // double buffered
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;
    }


    // get the best available match of pixel format for the device context
    format_index = ChoosePixelFormat(GetDC(window), &pfd);

    // make that the pixel format of the device context
    int succeeded = SetPixelFormat(GetDC(window), format_index, &pfd);

    if (!succeeded)
    {
        gl_log("Could not set pixel format\n");
        return 0;
    }

    HGLRC dummy_context = wglCreateContext(GetDC(window));
    if (!dummy_context)
    {
        gl_log("Could not create GL context. Exiting");
        return 0;
    }
    wglMakeCurrent(GetDC(window), dummy_context);
    if (!succeeded)
    {
        gl_log("Could not set current GL context. Exiting");
        return 0;
    }

    GLenum glew_result = glewInit();
    if (glew_result != GLEW_OK)
    {
        gl_log("Could not init glew.\n");
        return 0;
    }

    const int pixel_attribs[] =
    {
        WGL_ACCELERATION_ARB   , WGL_FULL_ACCELERATION_ARB           ,
        WGL_DRAW_TO_WINDOW_ARB , GL_TRUE           ,
        WGL_SUPPORT_OPENGL_ARB , GL_TRUE           ,
        WGL_DOUBLE_BUFFER_ARB  , GL_TRUE           ,
        WGL_PIXEL_TYPE_ARB     , WGL_TYPE_RGBA_ARB ,
        WGL_COLOR_BITS_ARB     , 32                ,
        WGL_DEPTH_BITS_ARB     , 24                ,
        WGL_STENCIL_BITS_ARB   , 8                 ,
        0                      ,
    };
    UINT num_formats = 0;

    int format_indices[20];

    wglChoosePixelFormatARB(GetDC(window),
            pixel_attribs, NULL, 20 /*max_formats*/, format_indices, &num_formats);
    if (!num_formats)
    {
        gl_log("Could not choose pixel format. Exiting.");
        return 0;
    }

    // The spec *guarantees* that this does not happen but nothing ever works...
    if (num_formats > 20)
    {
        num_formats = 20;
    }

    succeeded = 0;
    for (uint32_t i = 0; i < num_formats; ++i)
    {
        int local_index = format_indices[i];
        succeeded = SetPixelFormat(GetDC(window), local_index, &pfd);
        if (succeeded)
            break;
    }
    if (!succeeded)
    {
        gl_log("Could not set pixel format for final rendering context.\n");
        return 0;
    }

    const int context_attribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };
    *context = wglCreateContextAttribsARB(GetDC(window), 0/*shareContext*/,
            context_attribs);
    wglMakeCurrent(GetDC(window), *context);
    return 1;
}

#endif
