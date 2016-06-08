// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license
//


// Milton GPU renderer.
//
// Current Plan -- Draw a point (GL_POINTS) per point per stroke to develop the
// vertex shader. Make sure that zooming and panning works
//
//
//
// Rough outline for the future:
//
// The vertex shader will rasterize bounding slabs for each segment of the stroke
//  a    b      c    d
// o-----o------o----o (a four point stroke)
//
//    ___
// a|  / | b   the stroke gets drawn within slabs ab, bc, cd,
//  |/__|
//
// There will be overlap with each consecutive slab, so a problem to overcome
// will be overdraw. It is looking like the anti-aliasing solution for the GPU
// renderer will be simpler than the software renderer and it won't affect the
// rendering algorithm, but it is too early to tell.
//
// The vertex shader.
//
//      The slabs get updated, and they get interpolated onto the pixel shader.
//      Most of the hard work is in clipping and sending less work than is
//      necessary. Look into glMultiDraw* to send multiple VBOs.
//
//
// The pixel shader.
//
//      - To avoid overdraw, more than one option?
//          - Using the stencil buffer with one byte. Increment per stroke.
//          Early-out if stencil value equals current [0,255] value.
//          - ?? More ideas?
//      - Check distance to line. (1) get closest point. (2) euclidean dist.
//      (3) brush function
//      - If it is inside, blend color.
//
// Sandwich buffers.
//
//      Most of the time, we only need to update the working stroke.
//      Use cases.
//          - Painting. Update the working stroke. Layers above and below are the same.
//          - Panning: most of the screen can be copied!
//          - Zooming: Everything must be updated.
//          - Toggle layer visibility: ???
//
//      To start, always redraw everything.
//      Then, start keeping track of sandwich layers... do it incrementally.
//
//


struct RenderData
{
    GLuint program;
    GLuint vao;
};

bool hw_renderer_init(RenderData* render_data)
{
    bool result = true;
    // Load shader into opengl.
    GLuint objs[2];

    // TODO: In release, include the code directly.
#if MILTON_DEBUG
    size_t src_sz[2] = {0};
    char* src[2] =
    {
        debug_slurp_file("src/milton_canvas.v.glsl", &src_sz[0]),
        debug_slurp_file("src/milton_canvas.f.glsl", &src_sz[1]),
    };
    GLuint types[2] =
    {
        GL_VERTEX_SHADER,
        GL_FRAGMENT_SHADER
    };
#endif
    result = src_sz[0] != 0 && src_sz[1] != 0;

    assert(array_count(src) == array_count(objs));
    for (i64 i=0; i < array_count(src); ++i)
    {
        objs[i] = gl_compile_shader(src[i], types[i]);
    }

    render_data->program = glCreateProgram();

    gl_link_program(render_data->program, objs, array_count(objs));

    GLCHK( glUseProgram(render_data->program) );

    GLuint vbo = 0;

    GLCHK( glGenVertexArrays(1, &render_data->vao) );
    glBindVertexArray(render_data->vao);

    GLCHK( glGenBuffers(1, &vbo) );
    GLCHK( glBindBuffer(GL_ARRAY_BUFFER, vbo) );

    GLint pos_loc = glGetAttribLocation(render_data->program, "position");
    if (pos_loc >= 0)
    {
        GLfloat data[] =
        {
            -0.5, 0.5,
            -0.5, -0.5,
            0.5, -0.5,
        };
        GLCHK( glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW) );
        GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)pos_loc,
                                     /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                     /*stride*/0, /*ptr*/0));
        GLCHK( glEnableVertexAttribArray((GLuint)pos_loc) );
    }
    else
    {
        milton_log("HW Renderer problem: position location is not >=0\n");
    }
    glPointSize(10);

    return result;
}

void hw_render(RenderData* render_data)
{
    // Draw a triangle...
    GLCHK( glUseProgram(render_data->program) );
    glBindVertexArray(render_data->vao);
    //glDrawArrays(GL_TRIANGLES, 0, 3);
    glDrawArrays(GL_POINTS, 0, 3);
}

