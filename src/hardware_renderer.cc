// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license
//

#if MILTON_DEBUG
#define uniform
#define attribute
#define varying
#define in
#define out
#define flat
#define layout(param)
#define main vertexShaderMain
vec4 gl_Position;
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
ivec3 as_ivec3(vec3 v)
{
    ivec3 r;
    r.x = v.x;
    r.y = v.y;
    r.z = v.z;
    return r;
}
vec3 as_vec3(ivec3 v)
{
    vec3 r;
    r.x = v.x;
    r.y = v.y;
    r.z = v.z;
    return r;
}
vec4 as_vec4(ivec3 v)
{
    vec4 r;
    r.x = v.x;
    r.y = v.y;
    r.z = v.z;
    r.w = 1;
    return r;
}
vec4 as_vec4(vec3 v)
{
    vec4 r;
    r.x = v.x;
    r.y = v.y;
    r.z = v.z;
    r.w = 1;
    return r;
}
vec2 VEC2(ivec2 v)
{
    vec2 r;
    r.x = v.x;
    r.y = v.y;
    return r;
}
vec2 VEC2(float x,float y)
{
    vec2 r;
    r.x = x;
    r.y = y;
    return r;
}
vec3 VEC3(float x,float y,float z)
{
    vec3 r;
    r.x = x;
    r.y = y;
    r.z = z;
    return r;
}
vec4 VEC4(float v)
{
    vec4 r;
    r.x = v;
    r.y = v;
    r.z = v;
    r.w = v;
    return r;
}
vec4 VEC4(float x,float y,float z,float w)
{
    vec4 r;
    r.x = x;
    r.y = y;
    r.z = z;
    r.w = w;
    return r;
}
float distance(vec2 a, vec2 b)
{
    float dx = fabs(a.x-b.x);
    float dy = fabs(a.y-b.y);
    float d = dx*dx + dy*dy;
    if (d > 0) d = sqrtf(d);
    return d;
}
static vec4 gl_FragColor;
#pragma warning (push)
#pragma warning (disable : 4668)
#pragma warning (disable : 4200)
#define buffer struct
#include "common.glsl"
#include "milton_canvas.v.glsl"
#undef main
#define main fragmentShaderMain
#define v_pressure v_fragPressure
#define v_pointa v_fragpointa
#define v_pointb v_fragpointb
#define v_radii v_fragradii
struct StrokeBuffer
{
    vec3 points[STROKE_MAX_POINTS];
};
static StrokeBuffer StrokeBuffer;
#include "milton_canvas.f.glsl"
#pragma warning (pop)
#undef main
#undef attribute
#undef uniform
#undef buffer
#undef varying
#undef in
#undef out
#undef flat
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

#define PRESSURE_RESOLUTION (1<<20)

struct RenderData
{
    GLuint program;

    // Draw data for single stroke
    struct RenderElem
    {
        GLuint  vbo_quad;
        GLuint  vbo_pointa;
        GLuint  vbo_pointb;
        GLuint  vbo_radii;

        GLuint ubo_points;

        i64     count;
        // TODO: Store these two differently when collating multiple strokes...
        v4f     color;
        i32     radius;
    };
    DArray<RenderElem> render_elems;
};

// Load a shader and append line `#version 120`, which is invalid C++
#if MILTON_DEBUG
char* debug_load_shader_from_file(PATH_CHAR* path, size_t* out_size)
{
    char* contents = NULL;
    FILE* common_fd = platform_fopen(TO_PATH_STR("src/common.glsl"), TO_PATH_STR("r"));
    mlt_assert(common_fd);

    size_t bytes_in_common = bytes_in_fd(common_fd);

    char* common_contents = (char*)mlt_calloc(1, bytes_in_common);

    fread(common_contents, 1, bytes_in_common, common_fd);


    FILE* fd = platform_fopen(path, TO_PATH_STR("r"));

    if (fd)
    {
        char prelude[] = "#version 150\n";
        size_t prelude_len = strlen(prelude);
        size_t common_len = strlen(common_contents);
        size_t len = bytes_in_fd(fd);
        contents = (char*)mlt_calloc(len + 1 + common_len + prelude_len, 1);
        if (contents)
        {
            strcpy(contents, prelude);
            strcpy(contents+strlen(contents), common_contents);
            mlt_free(common_contents);

            char* file_data = (char*)mlt_calloc(len,1);
            size_t read = fread((void*)(file_data), 1, (size_t)len, fd);
            file_data[read] = '\0';
            strcpy(contents+strlen(contents), file_data);
            mlt_free(file_data);
            mlt_assert (read <= len);
            fclose(fd);
            if (out_size)
            {
                *out_size = strlen(contents)+1;
            }
            contents[*out_size] = '\0';
        }
    }
    else
    {
        if (out_size)
        {
            *out_size = 0;
        }
    }
    return contents;
}
#endif


bool gpu_init(RenderData* render_data)
{
    mlt_assert(PRESSURE_RESOLUTION == PRESSURE_RESOLUTION_GL);
    // TODO: Handle this. New MLT version?
    // mlt_assert(STROKE_MAX_POINTS == STROKE_MAX_POINTS_GL);

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
        debug_load_shader_from_file(TO_PATH_STR("src/milton_canvas.v.glsl"), &src_sz[0]),
        debug_load_shader_from_file(TO_PATH_STR("src/milton_canvas.f.glsl"), &src_sz[1]),
    };
    GLuint types[2] =
    {
        GL_VERTEX_SHADER,
        GL_FRAGMENT_SHADER
    };
#endif
    result = src_sz[0] != 0 && src_sz[1] != 0;

    mlt_assert(array_count(src) == array_count(objs));
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

static void gpu_set_background(RenderData* render_data, v3f background_color)
{
#if MILTON_DEBUG
    for(int i=0;i<3;++i) u_background_color.d[i] = background_color.d[i];
#endif
    gl_set_uniform_vec3(render_data->program, "u_background_color", 1, background_color.d);
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
    float fscreen[] = { (float)view->screen_size.x, (float)view->screen_size.y };
    gl_set_uniform_vec2(render_data->program, "u_screen_size", 1, fscreen);
    gl_set_uniform_i(render_data->program, "u_scale", view->scale);
}

void gpu_add_stroke(Arena* arena, RenderData* render_data, Stroke* stroke)
{
    vec2 cp;
    cp.x = stroke->points[stroke->num_points-1].x;
    cp.y = stroke->points[stroke->num_points-1].y;
#if MILTON_DEBUG
    if (u_scale != 0)
    {
        canvas_to_raster_gl(cp);
    }
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

    auto npoints = stroke->num_points;
    if (npoints == 1)
    {
        // TODO: handle special case...
        // So uncommon that adding an extra degenerate point might make sense,
        // if the algorithm can handle it
    }
    else
    {
        GLCHK( glUseProgram(render_data->program) );

        // 3 (triangle) *
        // 2 (two per segment) *
        // N-1 (segments per stroke)
        const size_t count_bounds = 3*2*((size_t)npoints-1);

        // 6 (3 * 2 from count_bounds)
        // N-1 (num segments)
        const size_t count_points = 6*((size_t)npoints-1);

        const size_t count_upoints = (size_t)npoints;

        v2i* bounds;
        v3i* apoints;
        v3i* bpoints;
        v3f* upoints;
        Arena scratch_arena = arena_push(arena, count_bounds*sizeof(decltype(*bounds))
                                         + 2*count_points*sizeof(decltype(*apoints))
                                         + count_upoints*sizeof(decltype(*upoints)));

        bounds  = arena_alloc_array(&scratch_arena, count_bounds, v2i);
        apoints = arena_alloc_array(&scratch_arena, count_bounds, v3i);
        bpoints = arena_alloc_array(&scratch_arena, count_bounds, v3i);
        upoints = arena_alloc_array(&scratch_arena, count_upoints, v3f);

        size_t bounds_i = 0;
        size_t apoints_i = 0;
        size_t bpoints_i = 0;
        size_t upoints_i = 0;
        for (i64 i=0; i < npoints-1; ++i)
        {
            v2i point_i = stroke->points[i];
            v2i point_j = stroke->points[i+1];

            Brush brush = stroke->brush;
            float radius_i = stroke->pressures[i]*brush.radius;
            float radius_j = stroke->pressures[i+1]*brush.radius;

            i32 min_x = min(point_i.x-radius_i, point_j.x-radius_j);
            i32 min_y = min(point_i.y-radius_i, point_j.y-radius_j);

            i32 max_x = max(point_i.x+radius_i, point_j.x+radius_j);
            i32 max_y = max(point_i.y+radius_i, point_j.y+radius_j);

            // Bounding rect
            //  Counter-clockwise
            //  TODO: Use triangle strips and flat shading to reduce redundant data
            bounds[bounds_i++] = { min_x, min_y };
            bounds[bounds_i++] = { min_x, max_y };
            bounds[bounds_i++] = { max_x, max_y };

            bounds[bounds_i++] = { max_x, max_y };
            bounds[bounds_i++] = { min_x, min_y };
            bounds[bounds_i++] = { max_x, min_y };

            // Pressures are in (0,1] but we need to encode them as integers.
            i32 pressure_a = (i32)(stroke->pressures[i] * (float)(PRESSURE_RESOLUTION));
            i32 pressure_b = (i32)(stroke->pressures[i+1] * (float)(PRESSURE_RESOLUTION));

            // one for every point in the triangle.
            for (int repeat = 0; repeat < 6; ++repeat)
            {
                apoints[apoints_i++] = { point_i.x, point_i.y, pressure_a };
                bpoints[bpoints_i++] = { point_j.x, point_j.y, pressure_b };
            }
            upoints[upoints_i++] = { (float)point_i.x, (float)point_i.y, (float)pressure_a };
            if (i == npoints-2)
            {
                upoints[upoints_i++] = { (float)point_j.x, (float)point_j.y, (float)pressure_b };
            }
        }
        mlt_assert(bounds_i == count_bounds);
        mlt_assert(bounds_i <= count_bounds);
        mlt_assert(apoints_i == bpoints_i);
        mlt_assert(bounds_i == apoints_i);

        auto upload_buffer = [](int* array, size_t count)
        {
            GLuint vbo = 0;
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            // Upload to GL
            GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count),
                                array,
                                GL_STATIC_DRAW) );
            return vbo;
        };

        GLuint vbo_quad   = upload_buffer((int*)bounds, bounds_i*sizeof(decltype(*bounds)));
        GLuint vbo_pointa = upload_buffer((int*)apoints, apoints_i*sizeof(decltype(*apoints)));
        GLuint vbo_pointb = upload_buffer((int*)bpoints, bpoints_i*sizeof(decltype(*bpoints)));
        gl_set_uniform_vec3i(render_data->program,
                             "u_stroke_points",
                             upoints_i,
                             (i32*)upoints);


        // Uniform buffer block for point array.
        GLuint ubo = 0;

        glGenBuffers(1, &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)(upoints_i*sizeof(decltype(*upoints))),
                     (float*)upoints, GL_STATIC_DRAW);

        RenderData::RenderElem re;
        re.vbo_quad = vbo_quad;
        re.vbo_pointa = vbo_pointa;
        re.vbo_pointb = vbo_pointb;
        re.ubo_points = ubo;
        re.count = (i64)bounds_i;
        re.color = { stroke->brush.color.r, stroke->brush.color.g, stroke->brush.color.b, stroke->brush.color.a };
        re.radius = stroke->brush.radius;
        push(&render_data->render_elems, re);
        arena_pop(&scratch_arena);
    }
}

void gpu_render(RenderData* render_data)
{
    // Draw a triangle...
    GLCHK( glUseProgram(render_data->program) );
    GLint loc = glGetAttribLocation(render_data->program, "a_position");
    GLint loc_a = glGetAttribLocation(render_data->program, "a_pointa");
    GLint loc_b = glGetAttribLocation(render_data->program, "a_pointb");
    if (loc >= 0)
    {
        for (size_t i = 0; i < render_data->render_elems.count; ++i)
        {
            auto re = render_data->render_elems.data[i];
            i64 count = re.count;

            // TODO. Only set these uniforms when both are different from the ones in use.
            gl_set_uniform_vec4(render_data->program, "u_brush_color", 1, re.color.d);
            gl_set_uniform_i(render_data->program, "u_radius", re.radius);
            GLuint uboi = glGetUniformBlockIndex(render_data->program, "StrokeUniformBlock");

            if (uboi != GL_INVALID_INDEX)
            {
                GLuint binding_point = 0;
                glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, re.ubo_points);
                glUniformBlockBinding(render_data->program, uboi, binding_point);
            }


            GLCHK( glBindBuffer(GL_ARRAY_BUFFER, re.vbo_quad) );
            GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                         /*size*/2, GL_INT, /*normalize*/GL_FALSE,
                                         /*stride*/0, /*ptr*/0));
            if (loc_a >=0)
            {
                GLCHK( glBindBuffer(GL_ARRAY_BUFFER, re.vbo_pointa) );
#if 0
                GLCHK( glVertexAttribIPointer(/*attrib location*/(GLuint)loc_a,
                                              /*size*/3, GL_INT,
                                              /*stride*/0, /*ptr*/0));
#else
                GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc_a,
                                             /*size*/3, GL_INT, /*normalize*/GL_FALSE,
                                             /*stride*/0, /*ptr*/0));
#endif
                GLCHK( glEnableVertexAttribArray((GLuint)loc_a) );
            }
            if (loc_b >=0)
            {
                GLCHK( glBindBuffer(GL_ARRAY_BUFFER, re.vbo_pointb) );
#if 0
                GLCHK( glVertexAttribIPointer(/*attrib location*/(GLuint)loc_b,
                                              /*size*/3, GL_INT,
                                              /*stride*/0, /*ptr*/0));
#else
                GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc_b,
                                             /*size*/3, GL_INT, /*normalize*/GL_FALSE,
                                             /*stride*/0, /*ptr*/0));
#endif
                GLCHK( glEnableVertexAttribArray((GLuint)loc_b) );
            }
            GLCHK( glEnableVertexAttribArray((GLuint)loc) );

            glDrawArrays(GL_TRIANGLES, 0, count);
        }
    }
    GLCHK (glUseProgram(0));
}

