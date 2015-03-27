#pragma once

#include "gl_helpers.h"
#include "libserg/serg_io.h"

typedef struct
{
    GLuint gl_quad_program;
    GLuint gl_texture;
    int width;
    int height;
} MiltonState;

void milton_init(MiltonState* milton_state)
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
        milton_state->gl_quad_program = glCreateProgram();
        gl_link_program(milton_state->gl_quad_program, shader_objects, 2);
        //GLuint shaders[2];
    }

    // Create texture
    // Create ray tracing target
    {
        GLCHK (glActiveTexture (GL_TEXTURE0) );
        // Create texture
        glGenTextures   (1, &milton_state->gl_texture);
        glBindTexture   (GL_TEXTURE_2D, milton_state->gl_texture);

        // Note for the future: These are needed.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        // Pass a null pointer, texture will be filled by opencl ray tracer
        GLCHK ( glTexImage2D(
                    GL_TEXTURE_2D, 0, GL_RGBA,  // GL_RGBA32F,
                    milton_state->width, milton_state->height,
                    0, GL_RGBA, GL_FLOAT, NULL) );
    }
}
