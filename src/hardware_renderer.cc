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

    // Dumb and stupid and temporary array
    DArray<GLuint> strokes;
};

bool gpu_init(RenderData* render_data)
{
    bool result = true;
    // Load shader into opengl.
    GLuint objs[2];

    // TODO: In release, include the code directly.
#if MILTON_DEBUG
    size_t src_sz[2] = {0};
    char* src[2] =
    {
        debug_slurp_file(TO_PATH_STR("src/milton_canvas.v.glsl"), &src_sz[0]),
        debug_slurp_file(TO_PATH_STR("src/milton_canvas.f.glsl"), &src_sz[1]),
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

    GLfloat data[] =
    {
        -0.5f, 0.5f,
        -0.5f, -0.5f,
        0.5f, -0.5f,
    };

    if (!gl_set_attribute_vec2(render_data->program, "position", data, sizeof(data)))
    {
        milton_log("HW Renderer problem: position location is not >=0\n");
    }
    glPointSize(10);

    return result;
}

void gpu_set_canvas(RenderData* render_data, CanvasView* view)
{
    // Shit we need:
    //  pan_vector      (vec2),
    //  scale           (int),
    //  screen_center   (vec2)
    gl_set_uniform_vec2i(render_data->program, "screen_center", 1, view->screen_center.d);
    gl_set_uniform_vec2i(render_data->program, "pan_vector", 1, view->pan_vector.d);
    gl_set_uniform_vec2i(render_data->program, "screen_size", 1, view->screen_size.d);
    gl_set_uniform_i(render_data->program, "pan_vector", view->scale);
}

void gpu_add_stroke(RenderData* render_data, Stroke* stroke)
{

}

void gpu_render(RenderData* render_data)
{
    // Draw a triangle...
    GLCHK( glUseProgram(render_data->program) );
    glBindVertexArray(render_data->vao);
    //glDrawArrays(GL_TRIANGLES, 0, 3);
    glDrawArrays(GL_POINTS, 0, 3);
    // Draw all strokes:
    //  For each stroke:
    //      For each point:
    //          Draw point!
}

