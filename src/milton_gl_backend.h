//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

// See gl_helpers.h for the reason for defining this
#if defined(__MACH__)
#define glGetAttribLocationARB glGetAttribLocation
#define glGetUniformLocationARB glGetUniformLocation
#define glUseProgramObjectARB glUseProgram
#define glCreateProgramObjectARB glCreateProgram
#define glEnableVertexAttribArrayARB glEnableVertexAttribArray
#define glVertexAttribPointerARB glVertexAttribPointer
#define glGenVertexArrays glGenVertexArraysAPPLE
#define glBindVertexArray glBindVertexArrayAPPLE
#endif

func void milton_gl_backend_draw(MiltonState* milton_state)
{
    MiltonGLState* gl = milton_state->gl;
    u8* raster_buffer = milton_state->raster_buffer;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)raster_buffer);

    GLCHK (glUseProgramObjectARB(gl->quad_program));
#if MILTON_USE_VAO
    glBindVertexArray(gl->quad_vao);
#else
    GLint pos_loc     = glGetAttribLocationARB(gl->quad_program, "position");
    GLint sampler_loc = glGetUniformLocationARB(gl->quad_program, "raster_buffer");
    assert (pos_loc     >= 0);
    assert (sampler_loc >= 0);
    GLCHK (glUniform1iARB(sampler_loc, 0 /*GL_TEXTURE0*/));
    GLCHK (glVertexAttribPointerARB(/*attrib location*/pos_loc,
                                    /*size*/2, GL_FLOAT,
                                    /*normalize*/GL_FALSE,
                                    /*stride*/0,
                                    /*ptr*/0));

    GLCHK (glEnableVertexAttribArrayARB(pos_loc));
#endif
    GLCHK (glDrawArrays (GL_TRIANGLE_FAN, 0, 4) );
}

func void milton_gl_set_brush_hover(MiltonGLState* gl,
                                    CanvasView* view,
                                    int radius,
                                    f32 x, f32 y)
{
    glUseProgramObjectARB(gl->quad_program);

    i32 width = view->screen_size.w;

    f32 radiusf = (f32)radius / (f32)width;

    GLint loc_hover_on  = glGetUniformLocationARB(gl->quad_program, "hover_on");
    GLint loc_radius_sq = glGetUniformLocationARB(gl->quad_program, "radiusf");
    GLint loc_pointer   = glGetUniformLocationARB(gl->quad_program, "pointer");
    GLint loc_aspect    = glGetUniformLocationARB(gl->quad_program, "aspect_ratio");
    assert ( loc_hover_on >= 0 );
    assert ( loc_radius_sq >= 0 );
    assert ( loc_pointer >= 0 );
    assert ( loc_aspect >= 0 );

    glUniform1iARB(loc_hover_on, 1);
    glUniform1fARB(loc_radius_sq, radiusf);
    glUniform2fARB(loc_pointer, x, y);
    glUniform1fARB(loc_aspect, view->aspect_ratio);
}

func void milton_gl_unset_brush_hover(MiltonGLState* gl)
{
    glUseProgramObjectARB(gl->quad_program);

    GLint loc_hover_on = glGetUniformLocationARB(gl->quad_program, "hover_on");
    assert ( loc_hover_on >= 0 );

    glUniform1iARB(loc_hover_on, 0);
}

func void milton_gl_backend_init(MiltonState* milton_state)
{
    // Init quad program
    {
        const char* shader_contents[2];
        shader_contents[0] =
            "#version 120\n"
            "attribute vec2 position;\n"
            "\n"
            "varying vec2 coord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "   coord = (position + vec2(1.0,1.0))/2.0;\n"
            "   coord.y = 1.0 - coord.y;"
            "   // direct to clip space. must be in [-1, 1]^2\n"
            "   gl_Position = vec4(position, 0.0, 1.0);\n"
            "}\n";

        shader_contents[1] =
            "#version 120\n"
            "\n"
            "uniform sampler2D raster_buffer;\n"
            "uniform bool hover_on;\n"
            "uniform vec2 pointer;\n"
            "uniform float radiusf;\n"
            "uniform float aspect_ratio;\n"
            "varying vec2 coord;\n"
            //"out vec4 out_color;\n"
            "\n"
            "void main(void)\n"
            "{\n"
            "   vec4 color = texture2D(raster_buffer, coord).bgra; \n"
            "   if (hover_on)\n"
            "   { \n"
            "       float girth = 0.001; \n"
            "       float dx = (coord.x - pointer.x); \n"
            "       float dy = (coord.y / aspect_ratio - pointer.y); \n"
            "       float dist = sqrt(dx * dx + dy * dy); \n"
            "       bool test1 = ( dist < radiusf + girth ); \n"
            "       bool test2 = ( dist > radiusf - girth ); \n"
            "       float t = float(test1 && test2); \n"
            "       color = t * vec4(0.5,0.5,0.5,1) + (1 - t) * color; \n"
            "   } \n"
            "   gl_FragColor = color; \n"
            //"   out_color = color; \n"
            "}\n";

        GLuint shader_objects[2] = {0};
        for ( int i = 0; i < 2; ++i )
        {
            GLuint shader_type = (i == 0) ? GL_VERTEX_SHADER_ARB : GL_FRAGMENT_SHADER_ARB;
            shader_objects[i] = gl_compile_shader(shader_contents[i], shader_type);
        }
        milton_state->gl->quad_program = glCreateProgramObjectARB();
        gl_link_program(milton_state->gl->quad_program, shader_objects, 2);

        GLCHK (glUseProgramObjectARB(milton_state->gl->quad_program));
    }

    // Create texture
    {
         GLCHK (glActiveTexture (GL_TEXTURE0) );
        // Create texture
        GLCHK (glGenTextures   (1, &milton_state->gl->texture));
        GLCHK (glBindTexture   (GL_TEXTURE_2D, milton_state->gl->texture));

        // Note for the future: These are needed.
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    }
    // Create quad
    {
#define u -1.0f
        // full
        GLfloat vert_data[] =
        {
            -u, +u,
            -u, -u,
            +u, -u,
            +u, +u,
        };
#undef u
#if MILTON_USE_VAO
        GLCHK (glGenVertexArrays(1, &milton_state->gl->quad_vao));
        GLCHK (glBindVertexArray(milton_state->gl->quad_vao));
#endif

        GLCHK (glGenBuffersARB(1, &milton_state->gl->vbo));
        assert (milton_state->gl->vbo > 0);
        GLCHK (glBindBufferARB(GL_ARRAY_BUFFER, milton_state->gl->vbo));

        GLint pos_loc     = glGetAttribLocationARB(milton_state->gl->quad_program, "position");
        GLint sampler_loc = glGetUniformLocationARB(milton_state->gl->quad_program, "raster_buffer");
        assert (pos_loc     >= 0);
        assert (sampler_loc >= 0);
        GLCHK (glBufferDataARB (GL_ARRAY_BUFFER, sizeof(vert_data), vert_data, GL_STATIC_DRAW));
#if MILTON_USE_VAO
        GLCHK (glVertexAttribPointerARB(/*attrib location*/pos_loc,
                                        /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                        /*stride*/0, /*ptr*/0));
        GLCHK (glEnableVertexAttribArrayARB (pos_loc) );
#endif
    }
}


