// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license
//

#if MILTON_DEBUG
#define uniform
#define attribute
#define main vertexShaderMain
struct Vec4_
{
    vec2 xy;
};
Vec4_ gl_Position;
vec2 as_vec2(ivec2 v)
{
    vec2 r;
    r.x = v.x;
    r.y = v.y;
    return r;
}
ivec2 as_ivec2(vec2 v)
{
    ivec2 r;
    r.x = v.x;
    r.y = v.y;
    return r;
}
#include "milton_canvas.v.glsl"
#undef main
#undef attribute
#undef uniform
#endif //MILTON_DEBUG

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

    // Dumb and stupid and temporary array
    struct RenderElem
    {
        GLuint vbo;
        i64    count;
    };
    DArray<RenderElem> render_elems;
};

bool gpu_init(RenderData* render_data)
{
    bool result = true;

#if MILTON_DEBUG
    // Assume our context is 3.0+
    // Create a single VAO and bind it.
    GLuint proxy_vao = 0;
    GLCHK( glGenVertexArrays(1, &proxy_vao) );
    GLCHK( glBindVertexArray(proxy_vao) );
#endif

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


    glPointSize(10);

    return result;
}

void gpu_update_scale(RenderData* render_data, i32 scale)
{
#if MILTON_DEBUG // set the shader values in C++
    u_scale = scale;
#endif
    gl_set_uniform_i(render_data->program, "u_scale", scale);
}

void gpu_set_canvas(RenderData* render_data, CanvasView* view)
{
#if MILTON_DEBUG // set the shader values in C++
#define COPY_VEC(a,b) a.x = b.x; a.y = b.y;
    COPY_VEC( u_pan_vector, view->pan_vector );
    COPY_VEC( u_screen_center, view->screen_center );
    COPY_VEC( u_screen_size, view->screen_size );
    u_scale = view->scale;
#undef COPY_VEC
#endif
    glUseProgram(render_data->program);
    gl_set_uniform_vec2i(render_data->program, "u_pan_vector", 1, view->pan_vector.d);
    gl_set_uniform_vec2i(render_data->program, "u_screen_center", 1, view->screen_center.d);
    gl_set_uniform_vec2(render_data->program, "u_screen_size", 1, view->screen_size.d);
    gl_set_uniform_i(render_data->program, "u_scale", view->scale);
}

void gpu_add_stroke(RenderData* render_data, Stroke* stroke)
{
    vec2 cp;
    cp.x = stroke->points[stroke->num_points-1].x;
    cp.y = stroke->points[stroke->num_points-1].y;
#if MILTON_DEBUG
    canvas_to_raster_gl(cp);
#endif
    //
    // Note.
    //  Sandwich buffers:
    //      The top and bottom layers should be two VBOs.
    //      The working layer will be a possible empty VBO\
    //      There will be a list of fresh VBOs of recently created strokes, the "ham"
    //      [ TOP LAYER ]  - Big VBO single draw call. Layers above working stroke.
    //          [  WL  ]    - Working layer. Single draw call for working stroke.
    //          [  Ham ]    - Ham. 0 or more draw calls for strokes waiting to get into a "bread" layer
    //      [ BOTTOM    ]  - Another big VBO, single draw call. Layers below working stroke.
    //
    // Create vbo for point.
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLCHK( glUseProgram(render_data->program) );
#if 1
    size_t sz = 2*sizeof(i32)*stroke->num_points;
    GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sz),
                        (int*)stroke->points,
                        GL_STATIC_DRAW) );
#else
    GLfloat data[] =
    {
        (float)stroke->points[stroke->num_points-1].x,
        (float)stroke->points[stroke->num_points-1].y,
    };
    GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(data)), data, GL_STATIC_DRAW) );
#endif
    RenderData::RenderElem re;
    re.vbo = vbo;
    re.count = stroke->num_points;
    push(&render_data->render_elems, re);
}


void gpu_render(RenderData* render_data)
{
    // Draw a triangle...
    GLCHK( glUseProgram(render_data->program) );
    GLint loc = glGetAttribLocation(render_data->program, "a_position");
    if (loc >= 0)
    {
        for (size_t i = 0; i < render_data->render_elems.count; ++i)
        {
            auto re = render_data->render_elems.data[i];
            GLuint stroke = re.vbo;
            i64 count = re.count;
            GLCHK( glBindBuffer(GL_ARRAY_BUFFER, stroke) );
            //GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
            //                             /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
            //                             /*stride*/0, /*ptr*/0));
            GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                         /*size*/2, GL_INT, /*normalize*/GL_FALSE,
                                         /*stride*/0, /*ptr*/0));
            GLCHK( glEnableVertexAttribArray((GLuint)loc) );

            glDrawArrays(GL_POINTS, 0, count);
        }
    }
    GLCHK (glUseProgram(0));
}

