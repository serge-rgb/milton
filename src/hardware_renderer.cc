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
ivec3 IVEC3(i32 x,i32 y,i32 z)
{
    ivec3 r;
    r.x = x;
    r.y = y;
    r.z = z;
    return r;
}
vec3 VEC3(float v)
{
    vec3 r;
    r.x = v;
    r.y = v;
    r.z = v;
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
ivec4 IVEC4(i32 x, i32 y, i32 z, i32 w)
{
    ivec4 r;
    r.x = x;
    r.y = y;
    r.z = z;
    r.w = w;
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

static vec2 gl_PointCoord;
static vec4 gl_FragColor;

// Conforming to std140 layout.
struct StrokeUniformData
{
    int count;
    vec3 pad_;
    ivec4 points[512]; // TODO: deal with STROKE_MAX_POINTS_GL
};
StrokeUniformData u_stroke;

#pragma warning (push)
#pragma warning (disable : 4668)
#pragma warning (disable : 4200)
#define buffer struct
//#include "common.glsl"
//#include "milton_canvas.v.glsl"
#undef main
#define main fragmentShaderMain
#define texture2D(a,b) VEC4(0)
#define sampler2D int
//#include "milton_canvas.f.glsl"
#pragma warning (pop)
#undef texture2D
#undef main
#undef attribute
#undef uniform
#undef buffer
#undef varying
#undef in
#undef out
#undef flat
#undef sampler2D
#endif //MILTON_DEBUG

// Milton GPU renderer.
//
//
// Rough outline:
//
// The vertex shader rasterizes bounding slabs for each segment of the stroke
//  a    b      c    d
// o-----o------o----o (a four point stroke)
//
//    ___
// a|  / | b   the stroke gets drawn within slabs ab, bc, cd,
//  |/__|
//
//
// The vertex shader: Translate from raster to canvas (i.e. do zoom&panning).
//
// The pixel shader.
//
//      - Check distance to line. (1) get closest point. (2) euclidean dist.
//      (3) brush function
//      - If it is inside, blend color.
//
//
//
//
// == Future?
//
// 1. Sandwich buffers.
//
//      We can render to two textures. One for strokes below the working stroke
//      and one for strokes above. As a final canvas rendering pass, we render
//      the working stroke and blend it between the two textures.
//      This would make the common case for rendering much faster.
//
//
// 2. Chunks
//
//      Each stroke uses 8KB of GPU memory just for the point data. For 10,000
//      strokes at 128 points per stroke, this translates into 19MB. To set an
//      upper bound on the GPU memory requirement for Milton, we could render
//      in chunks of N strokes, where N is some big number. At the end of each
//      chunk, we can clear all VBOs and UBOs and be ready for the next one.
//

#define PRESSURE_RESOLUTION (1<<20)

struct RenderData
{
    GLuint stroke_program;
    GLuint quad_program;

    // VBO for the screen-covering quad.
    GLuint vbo_quad;
    GLuint vbo_quad_uv;

    GLuint canvas_texture;

    GLuint fbo;

    // Draw data for single stroke
    struct RenderElem
    {
        GLuint  vbo_stroke;

        GLuint ubo_stroke;

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


bool gpu_init(RenderData* render_data, CanvasView* view)
{
    //mlt_assert(PRESSURE_RESOLUTION == PRESSURE_RESOLUTION_GL);
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
    {
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

        render_data->stroke_program = glCreateProgram();

        gl_link_program(render_data->stroke_program, objs, array_count(objs));

        GLCHK( glUseProgram(render_data->stroke_program) );
        gl_set_uniform_i(render_data->stroke_program, "u_canvas", /*GL_TEXTURE1*/2);
    }

    // Quad for screen!
    {
        // a------d
        // |  \   |
        // |    \ |
        // b______c
        //  Triangle fan:
        GLfloat quad_data[] =
        {
            -1 , -1 , // a
            -1 , 1  , // b
            1  , 1  , // c
            1  , -1 , // d
        };

        // Create buffers and upload
        GLuint vbo = 0;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, array_count(quad_data)*sizeof(*quad_data), quad_data, GL_STATIC_DRAW);

        GLfloat uv_data[] =
        {
            0,0,
            0,1,
            1,1,
            1,0,
        };
        GLuint vbo_uv = 0;
        glGenBuffers(1, &vbo_uv);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_uv);
        glBufferData(GL_ARRAY_BUFFER, array_count(uv_data)*sizeof(*uv_data), uv_data, GL_STATIC_DRAW);


        render_data->vbo_quad = vbo;
        render_data->vbo_quad_uv = vbo_uv;

        char vsrc[] =
                "#version 120 \n"
                "attribute vec2 a_point; \n"
                "attribute vec2 a_uv; \n"
                "varying vec2 v_uv; \n"
                "void main() { \n"
                    "v_uv = a_uv; \n"
                "    gl_Position = vec4(a_point, 0,1); \n"
                "} \n";
        char fsrc[] =
                "#version 120 \n"
                "uniform sampler2D u_canvas; \n"
                "varying vec2 v_uv; \n"
                "void main() \n"
                "{ \n"
                    "vec4 color = texture2D(u_canvas, v_uv); \n"
                    "gl_FragColor = color; \n"
                "} \n";

        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(vsrc, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(fsrc, GL_FRAGMENT_SHADER);
        render_data->quad_program = glCreateProgram();
        gl_link_program(render_data->quad_program, objs, array_count(objs));
    }

    // Framebuffer object for canvas
    {
        glActiveTexture(GL_TEXTURE2);
        GLCHK (glGenTextures(1, &render_data->canvas_texture));
        glBindTexture(GL_TEXTURE_2D, render_data->canvas_texture);

        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

        u8* foo = (u8*)mlt_calloc(1, (size_t)(view->screen_size.w*view->screen_size.h*8));
        glTexImage2D(GL_TEXTURE_2D,
                     0, GL_RGBA,
                     view->screen_size.w, view->screen_size.h,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
        //GLCHK( glTexImage2D(GL_TEXTURE_2D, 0, /*internalFormat, num of components*/GL_RGBA8,
        //                    view->screen_size.w, view->screen_size.h,
        //                    /*border*/0, /*pixel_data_format*/GL_BGRA,
        //                    /*component type*/GL_UNSIGNED_BYTE,
        //                    foo) );
        //                    //NULL) );
        mlt_free(foo);

        GLuint fbo = 0;
        glGenFramebuffersEXT(1, &fbo);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
        GLCHK( glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D,
                                         render_data->canvas_texture, 0) );

        GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        char* msg = NULL;
        switch (status)
        {
        case GL_FRAMEBUFFER_COMPLETE_EXT:
            {
                // OK!
                break;
            }
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
            {
                msg = "Incomplete Attachment";
                break;
            }
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
            {
                msg = "Missing Attachment";
                break;
            }
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
            {
                msg = "Incomplete Draw Buffer";
                break;
            }
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
            {
                msg = "Incomplete Read Buffer";
                break;
            }
        case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
            {
                msg = "Unsupported Framebuffer";
                break;
            }
        default:
            {
                msg = "Unknown";
                break;
            }
        }

        if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
        {
            char warning[1024];
            snprintf(warning, "Framebuffer Error: %s", msg);

        }

        render_data->fbo = fbo;
    }

    return result;
}

void gpu_resize(RenderData* render_data, CanvasView* view)
{
    // Create canvas texture
    glActiveTexture(GL_TEXTURE2);
    GLCHK (glBindTexture(GL_TEXTURE_2D, render_data->canvas_texture));


    // TODO: Use 32-bit per channel data if we move toward multiple textures.


#if 0
    // Find the next size that is a power of two.
    i32 pow_w = 0;
    i32 pow_h = 0;
    BIT_SCAN_REVERSE(view->screen_size.w, pow_w);
    BIT_SCAN_REVERSE(view->screen_size.h, pow_h);
    if (1<<pow_w < view->screen_size.w) pow_w++;
    if (1<<pow_h < view->screen_size.h) pow_h++;
    i32 tex_w = 1<<pow_w;
    i32 tex_h = 1<<pow_h;
#else
    // OpenGL 2.1+ lets us have non-power-of-two textures
    i32 tex_w = view->screen_size.w;
    i32 tex_h = view->screen_size.h;
#endif

    size_t sz = (size_t)(tex_w*tex_h*4);

    u32* color_buffer = (u32*)mlt_calloc(1, sz);


    if (color_buffer)
    {
        b32 parity = 1;
        for (i32 y=0; y < tex_h; ++y)
        {
            for (i32 x = 0; x < tex_w; ++x)
            {
                //u32 color = parity? 0xffff00ff : 0xff00ff00;
                u32 color = 0xff000000;
                color_buffer[y*tex_w + x] = color;
                parity = (parity+1)%2;
            }
        }

        GLCHK (glTexImage2D(GL_TEXTURE_2D, 0, /*internalFormat, num of components*/GL_RGBA8,
                            tex_w, tex_h,
                            /*border*/0, /*pixel_data_format*/GL_BGRA,
                            /*component type*/GL_UNSIGNED_BYTE, (GLvoid*)color_buffer));

        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

        mlt_free(color_buffer);

        gl_set_uniform_i(render_data->quad_program, "u_canvas", /*GL_TEXTURE1*/2);
    }
    else
    {
        // TODO: handle out of memory error
        mlt_assert(color_buffer);
    }
}

void gpu_update_scale(RenderData* render_data, i32 scale)
{
#if MILTON_DEBUG // set the shader values in C++
    // Shader
    // u_scale = scale;
#endif
    gl_set_uniform_i(render_data->stroke_program, "u_scale", scale);
}

static void gpu_set_background(RenderData* render_data, v3f background_color)
{
#if MILTON_DEBUG
    // SHADER
    // for(int i=0;i<3;++i) u_background_color.d[i] = background_color.d[i];
#endif
    gl_set_uniform_vec3(render_data->stroke_program, "u_background_color", 1, background_color.d);
}

void gpu_set_canvas(RenderData* render_data, CanvasView* view)
{
#if MILTON_DEBUG // set the shader values in C++
#define COPY_VEC(a,b) a.x = b.x; a.y = b.y;
    // SHADER
    //COPY_VEC( u_pan_vector, view->pan_vector );
    //COPY_VEC( u_screen_center, view->screen_center );
    //COPY_VEC( u_screen_size, view->screen_size );
    //u_scale = view->scale;
#undef COPY_VEC
#endif
    glUseProgram(render_data->stroke_program);
    gl_set_uniform_vec2i(render_data->stroke_program, "u_pan_vector", 1, view->pan_vector.d);
    gl_set_uniform_vec2i(render_data->stroke_program, "u_screen_center", 1, view->screen_center.d);
    float fscreen[] = { (float)view->screen_size.x, (float)view->screen_size.y };
    gl_set_uniform_vec2(render_data->stroke_program, "u_screen_size", 1, fscreen);
    gl_set_uniform_i(render_data->stroke_program, "u_scale", view->scale);
}

void gpu_add_stroke(Arena* arena, RenderData* render_data, Stroke* stroke)
{
    vec2 cp;
    cp.x = stroke->points[stroke->num_points-1].x;
    cp.y = stroke->points[stroke->num_points-1].y;
#if MILTON_DEBUG
    // SHADER
    //if (u_scale != 0)
    //{
    //    canvas_to_raster_gl(cp);
    //}
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
        GLCHK( glUseProgram(render_data->stroke_program) );

        // 3 (triangle) *
        // 2 (two per segment) *
        // N-1 (segments per stroke)
        const size_t count_bounds = 3*2*((size_t)npoints-1);

        // 6 (3 * 2 from count_bounds)
        // N-1 (num segments)
        const size_t count_points = 6*((size_t)npoints-1);

        const size_t count_upoints = (size_t)npoints;

        v2i* bounds;
        StrokeUniformData* uniform_data;
        Arena scratch_arena = arena_push(arena, count_bounds*sizeof(decltype(*bounds)) + sizeof(StrokeUniformData));

        bounds  = arena_alloc_array(&scratch_arena, count_bounds, v2i);
        uniform_data = arena_alloc_elem(&scratch_arena, StrokeUniformData);

        size_t bounds_i = 0;
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

            uniform_data->points[uniform_data->count++] = IVEC4( point_i.x, point_i.y, pressure_a, 0 );
            if (i == npoints-2)
            {
                uniform_data->points[uniform_data->count++] = IVEC4( point_j.x, point_j.y, pressure_b, 0 );
            }
        }

        mlt_assert(bounds_i == count_bounds);

        GLuint vbo_stroke = 0;
        glGenBuffers(1, &vbo_stroke);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_stroke);
        // Upload to GL
        GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bounds))),
                            bounds,
                            GL_STATIC_DRAW) );


        // Uniform buffer block for point array.
        GLuint ubo = 0;

        glGenBuffers(1, &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)(sizeof(decltype(*uniform_data))),
                     uniform_data, GL_STATIC_DRAW);

        RenderData::RenderElem re;
        re.vbo_stroke = vbo_stroke;
        re.ubo_stroke = ubo;
        re.count = (i64)bounds_i;
        re.color = { stroke->brush.color.r, stroke->brush.color.g, stroke->brush.color.b, stroke->brush.color.a };
        re.radius = stroke->brush.radius;
        push(&render_data->render_elems, re);
        arena_pop(&scratch_arena);
    }
}

void gpu_render(RenderData* render_data)
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, render_data->fbo);
    // Draw the canvas
    {
        GLCHK( glUseProgram(render_data->stroke_program) );
        GLint loc = glGetAttribLocation(render_data->stroke_program, "a_position");
        if (loc >= 0)
        {
            for (size_t i = 0; i < render_data->render_elems.count; ++i)
            {
                auto re = render_data->render_elems.data[i];
                i64 count = re.count;

                // TODO. Only set these uniforms when both are different from the ones in use.
                gl_set_uniform_vec4(render_data->stroke_program, "u_brush_color", 1, re.color.d);
                gl_set_uniform_i(render_data->stroke_program, "u_radius", re.radius);

                // Bind stroke uniform block
                GLuint uboi = glGetUniformBlockIndex(render_data->stroke_program, "StrokeUniformBlock");
                if (uboi != GL_INVALID_INDEX)
                {
                    GLuint binding_point = 0;
                    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, re.ubo_stroke);
                    glUniformBlockBinding(render_data->stroke_program, uboi, binding_point);
                }


                GLCHK( glBindBuffer(GL_ARRAY_BUFFER, re.vbo_stroke) );
                GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                             /*size*/2, GL_INT, /*normalize*/GL_FALSE,
                                             /*stride*/0, /*ptr*/0));
                GLCHK( glEnableVertexAttribArray((GLuint)loc) );

                glDrawArrays(GL_TRIANGLES, 0, count);

                // TODO: is there any way to avoid the memory barrier in order to lower the GL requirements?
                glMemoryBarrierEXT(GL_TEXTURE_FETCH_BARRIER_BIT_EXT);
                /* glTextureBarrier(); */
            }
        }
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    // Render quad for whole screen.
    {
        // Bind canvas texture.
        glUseProgram(render_data->quad_program);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, render_data->canvas_texture);

        GLint loc = glGetAttribLocation(render_data->quad_program, "a_point");
        if (loc >= 0)
        {
            GLint loc_uv = glGetAttribLocation(render_data->quad_program, "a_uv");
            if (loc_uv >=0)
            {
                GLCHK( glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_quad_uv) );
                GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc_uv,
                                             /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                             /*stride*/0, /*ptr*/0));
                glEnableVertexAttribArray((GLuint)loc_uv);
            }


            GLCHK( glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_quad) );
            GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                         /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                         /*stride*/0, /*ptr*/0));
            glEnableVertexAttribArray((GLuint)loc);
            glDrawArrays(GL_TRIANGLE_FAN,0,4);
        }

    }
    GLCHK (glUseProgram(0));
}

