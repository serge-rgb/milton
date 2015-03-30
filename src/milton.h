#pragma once

#include "gl_helpers.h"
#include "libserg/serg_io.h"

#define GL_BACKEND_STUB 0

typedef int32_t bool32;

#if GL_BACKEND_STUB
typedef struct
{
    GLuint quad_program;
    GLuint texture;
    GLuint quad_vao;
} MiltonGLState;
#endif

typedef struct
{
    int32_t     width;
    int32_t     height;
    uint8_t     bytes_per_pixel;
    uint8_t*    raster_buffer;
} MiltonState;

#if GL_BACKEND_STUB
static void gl_backend_init(MiltonState* milton_state)
{
    // Init quad program
    {
        const char* shader_paths[2] =
        {
            "src/quad.v.glsl",
            "src/quad.f.glsl",
        };

        GLuint shader_objects[2] = {0};
        for ( int i = 0; i < 2; ++i )
        {
            int size;
            const char* shader_contents = slurp_file(shader_paths[i], &size);
            GLuint shader_type = (i == 0) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
            shader_objects[i] = gl_compile_shader(shader_contents, shader_type);
        }
        milton_state->gl->quad_program = glCreateProgram();
        gl_link_program(milton_state->gl->quad_program, shader_objects, 2);
        //GLuint shaders[2];
    }

    // Create texture
    // Create ray tracing target
    {
        GLCHK (glActiveTexture (GL_TEXTURE0) );
        // Create texture
        GLCHK (glGenTextures   (1, &milton_state->gl->texture));
        GLCHK (glBindTexture   (GL_TEXTURE_2D, milton_state->gl->texture));

        // Note for the future: These are needed.
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));

        // Pass a null pointer, texture will be filled by opencl ray tracer
        GLCHK ( glTexImage2D(
                    GL_TEXTURE_2D, 0, GL_RGBA32F,
                    milton_state->width, milton_state->height,
                    0, GL_RGBA, GL_FLOAT, NULL) );
    }
    // Create quad
    {
        //const GLfloat u = 1.0f;
#define u -1.0f
        // full
        GLfloat vert_data[] =
        {
            -u, u,
            -u, -u,
            u, -u,
            u, u,
        };
#undef u
        GLCHK (glGenVertexArrays(1, &milton_state->gl->quad_vao));
        GLCHK (glBindVertexArray(milton_state->gl->quad_vao));

        GLuint vbo;
        GLCHK (glGenBuffers(1, &vbo));
        GLCHK (glBindBuffer(GL_ARRAY_BUFFER, vbo));

        GLCHK (glBufferData (GL_ARRAY_BUFFER, sizeof(vert_data), vert_data, GL_STATIC_DRAW));
        GLCHK (glEnableVertexAttribArray (0) );
        GLCHK (glVertexAttribPointer     (/*attrib location*/0,
                    /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE, /*stride*/0, /*ptr*/0));
    }
}

static void gl_backend_draw(MiltonState* milton_state)
{
    MiltonGLState* gl = milton_state->gl;
    glUseProgram(gl->quad_program);
    glBindVertexArray(gl->quad_vao);
    GLCHK (glDrawArrays (GL_TRIANGLE_FAN, 0, 4) );
}
#endif // GL_BACKEND_STUB

static void milton_init(Arena* root_arena, MiltonState* milton_state)
{
#if GL_BACKEND_STUB
    milton_state->width = 1024;
    milton_state->height = 768;
    MiltonGLState* gl = arena_alloc_elem(root_arena, MiltonGLState);
    milton_state->gl = gl;
    gl_backend_init(milton_state);
#endif
    // Allocate a "big enough" raster_buffer to use for screen blitting.
    // 10k resolution is more than any monitor in the market right now .
    milton_state->width     = 10000;
    milton_state->height    = 10000;
    milton_state->bytes_per_pixel       = 4;
    milton_state->raster_buffer = arena_alloc_array(root_arena,
            milton_state->width * milton_state->height * milton_state->bytes_per_pixel,
            uint8_t);
}
