// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "shaders.gen.h"

#include "color.h"
#include "gl_helpers.h"
#include "gui.h"
#include "milton.h"
#include "vector.h"

#define PRESSURE_RESOLUTION (1<<20)

#define MAX_DEPTH_VALUE (1<<20)     // Strokes have MAX_DEPTH_VALUE different z values. 1/i for each i in [0, MAX_DEPTH_VALUE)
                                    // Also defined in stroke_raster.v.glsl
                                    //
                                    // NOTE: Using this technique means that the algorithm is not correct.
                                    //  There is a low probability that one stroke will cover another
                                    //  stroke with the same z value.

struct RenderData
{
    f32 viewport_limits[2];  // OpenGL limits to the framebuffer size.

    // OpenGL programs.
    GLuint stroke_program;
    GLuint quad_program;
    GLuint picker_program;
    GLuint layer_blend_program;
    GLuint outline_program;
    GLuint exporter_program;
    GLuint texture_fill_program;
    GLuint postproc_program;
#if MILTON_DEBUG
    GLuint simple_program;
#endif

    // VBO for the screen-covering quad.
    GLuint vbo_screen_quad;

    // Handles for color picker.
    GLuint vbo_picker;
    GLuint vbo_picker_norm;

    // Handles for brush outline.
    GLuint vbo_outline;
    GLuint vbo_outline_sizes;

    // Handles for exporter rectangle.
    GLuint vbo_exporter[4]; // One for each line in rectangle

    // Objects used in rendering.
    GLuint canvas_texture;
    GLuint eraser_texture;
    GLuint helper_texture;  // Used for various effects..
    GLuint stencil_texture;
    GLuint fbo;

    i32 flags;  // RenderDataFlags enum

    DArray<RenderElement> clip_array;

    // Screen size.
    i32 width;
    i32 height;

    v3f background_color;
    i32 scale;

    // See MAX_DEPTH_VALUE
    i32 stroke_z;

    // Cached values for stroke rendering uniforms.
    v4f current_color;
    float current_radius;

#if MILTON_ENABLE_PROFILING
    i32 clipped_count;  // Number of strokes in GPU memory.
#endif
};

enum RenderElementFlags
{
    RenderElementFlags_NONE = 0,

    RenderElementFlags_LAYER            = 1<<0,
};

enum GLVendor
{
    GLVendor_NVIDIA,
    GLVendor_INTEL,
    GLVendor_AMD,
    GLVendor_UNKNOWN,
};

char DEBUG_g_buffers[100000];

static void
DEBUG_gl_mark_buffer(GLuint buffer)
{
    mlt_assert(buffer < 100000);
    DEBUG_g_buffers[buffer] = 1;
}

static void
DEBUG_gl_unmark_buffer(GLuint buffer)
{
    mlt_assert(buffer < 100000);
    DEBUG_g_buffers[buffer] = 0;
}

static void
DEBUG_gl_validate_buffer(GLuint buffer)
{
    mlt_assert(buffer < 100000);
    mlt_assert(DEBUG_g_buffers[buffer]);
}

static void
print_framebuffer_status()
{
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    char* msg = NULL;
    switch ( status ) {
        case GL_FRAMEBUFFER_COMPLETE: {
            // OK!
            break;
        }
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: {
            msg = "Incomplete Attachment";
            break;
        }
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: {
            msg = "Missing Attachment";
            break;
        }
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: {
            msg = "Incomplete Draw Buffer";
            break;
        }
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: {
            msg = "Incomplete Read Buffer";
            break;
        }
        case GL_FRAMEBUFFER_UNSUPPORTED: {
            msg = "Unsupported Framebuffer";
            break;
        }
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: {
            msg = "Incomplete Multisample";
            break;
        }
        default: {
            msg = "Unknown";
            break;
        }
    }

    if ( status != GL_FRAMEBUFFER_COMPLETE ) {
        char warning[1024];
        snprintf(warning, 1024, "Framebuffer Error: %s", msg);
        milton_log("Warning %s\n", warning);
    }
}

RenderData*
gpu_allocate_render_data(Arena* arena)
{
    RenderData* p = arena_alloc_elem(arena, RenderData);
    return p;
}

// Send Color Picker data to OpenGL.
void
gpu_update_picker(RenderData* render_data, ColorPicker* picker)
{
    glUseProgram(render_data->picker_program);
    // Transform to [-1,1]
    v2f a = picker->data.a;
    v2f b = picker->data.b;
    v2f c = picker->data.c;
    Rect bounds = picker_get_bounds(picker);
    int w = bounds.right-bounds.left;
    int h = bounds.bottom-bounds.top;
    // The center of the picker has an offset of (20,30)
    // and the bounds radius is 100 px
    auto transform = [&](v2f p) { return v2f{2*p.x/w-1 - 0.20f, 2*p.y/h-1 -0.30f}; };
    a = transform(a);
    b = transform(b);
    c = transform(c);
    gl_set_uniform_vec2(render_data->picker_program, "u_pointa", 1, a.d);
    gl_set_uniform_vec2(render_data->picker_program, "u_pointb", 1, b.d);
    gl_set_uniform_vec2(render_data->picker_program, "u_pointc", 1, c.d);
    gl_set_uniform_f(render_data->picker_program, "u_angle", picker->data.hsv.h);

    v3f hsv = picker->data.hsv;
    gl_set_uniform_vec3(render_data->picker_program, "u_color", 1, hsv_to_rgb(hsv).d);

    // Point within triangle
    {
        v2f point = lerp2f(picker->data.b, lerp2f(picker->data.a, picker->data.c, hsv.s), hsv.v);
        // Move to [-1,1]^2
        point = transform(point);
        gl_set_uniform_vec2(render_data->picker_program, "u_triangle_point", 1, point.d);
    }
    v4f colors[5] = {};
    ColorButton* button = picker->color_buttons;
    colors[0] = button->rgba; button = button->next;
    colors[1] = button->rgba; button = button->next;
    colors[2] = button->rgba; button = button->next;
    colors[3] = button->rgba; button = button->next;
    colors[4] = button->rgba; button = button->next;
    gl_set_uniform_vec4(render_data->picker_program, "u_colors", 5, (float*)colors);

    // Update VBO for picker
    {
        Rect rect = get_bounds_for_picker_and_colors(picker);
        // convert to clip space
        v2i screen_size = {render_data->width, render_data->height};
        float top = (float)rect.top / screen_size.h;
        float bottom = (float)rect.bottom / screen_size.h;
        float left = (float)rect.left / screen_size.w;
        float right = (float)rect.right / screen_size.w;
        top = (top*2.0f - 1.0f) * -1;
        bottom = (bottom*2.0f - 1.0f) *-1;
        left = left*2.0f - 1.0f;
        right = right*2.0f - 1.0f;
        // a------d
        // |  \   |
        // |    \ |
        // b______c
        GLfloat data[] =
        {
            left, top,
            left, bottom,
            right, bottom,
            right, top,
        };
        float ratio = (float)(rect.bottom-rect.top) / (float)(rect.right-rect.left);
        ratio = (ratio*2)-1;
        // normalized positions.
        GLfloat norm[] =
        {
            -1, -1,
            -1, ratio,
            1, ratio,
            1, -1,
        };

        // Create buffers and upload
        glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_picker);
        DEBUG_gl_mark_buffer(render_data->vbo_picker);
        glBufferData(GL_ARRAY_BUFFER, array_count(data)*sizeof(*data), data, GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_picker_norm);
        DEBUG_gl_mark_buffer(render_data->vbo_picker_norm);
        glBufferData(GL_ARRAY_BUFFER, array_count(norm)*sizeof(*norm), norm, GL_STATIC_DRAW);
    }
}

void
gpu_update_brush_outline(RenderData* render_data, i32 cx, i32 cy, i32 radius,
                         BrushOutlineEnum outline_enum, v4f color)
{
    if ( render_data->vbo_outline == 0 ) {
        mlt_assert(render_data->vbo_outline_sizes == 0);
        glGenBuffers(1, &render_data->vbo_outline);
        glGenBuffers(1, &render_data->vbo_outline_sizes);
    }
    mlt_assert(render_data->vbo_outline_sizes != 0);

    float radius_plus_girth = radius + 4.0f; // Girth defined in outline.f.glsl

    auto w = render_data->width;
    auto h = render_data->height;

    // Normalized to [-1,1]
    GLfloat data[] = {
        2*((cx-radius_plus_girth) / w)-1,  -2*((cy-radius_plus_girth) / h)+1,
        2*((cx-radius_plus_girth) / w)-1,  -2*((cy+radius_plus_girth) / h)+1,
        2*((cx+radius_plus_girth) / w)-1,  -2*((cy+radius_plus_girth) / h)+1,
        2*((cx+radius_plus_girth) / w)-1,  -2*((cy-radius_plus_girth) / h)+1,
    };

    GLfloat sizes[] = {
        -radius_plus_girth, -radius_plus_girth,
        -radius_plus_girth,  radius_plus_girth,
         radius_plus_girth,  radius_plus_girth,
         radius_plus_girth, -radius_plus_girth,
    };

    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_outline_sizes);
    DEBUG_gl_mark_buffer(render_data->vbo_outline_sizes);
    glBufferData(GL_ARRAY_BUFFER, array_count(sizes)*sizeof(*sizes), sizes, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_outline);
    DEBUG_gl_mark_buffer(render_data->vbo_outline);
    GLCHK( glBufferData(GL_ARRAY_BUFFER, array_count(data)*sizeof(*data), data, GL_DYNAMIC_DRAW) );

    gl_set_uniform_i(render_data->outline_program, "u_radius", radius);
    if ( outline_enum == BrushOutline_FILL ) {
        gl_set_uniform_i(render_data->outline_program, "u_fill", true);
        gl_set_uniform_vec4(render_data->outline_program, "u_color", 1, color.d);
    }
    else if ( outline_enum == BrushOutline_NO_FILL ) {
        gl_set_uniform_i(render_data->outline_program, "u_fill", false);
    }
}

b32
gpu_init(RenderData* render_data, CanvasView* view, ColorPicker* picker, i32 render_data_flags)
{
    render_data->stroke_z = MAX_DEPTH_VALUE - 20;

    if ( gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        glEnable(GL_MULTISAMPLE);
        if ( gl_helper_check_flags(GLHelperFlags_SAMPLE_SHADING) ) {
            glEnable(GL_SAMPLE_SHADING_ARB);
            GLCHK( glMinSampleShadingARB(1.0f) );
        }
    }

    {
        GLfloat viewport_dims[2] = {};
        glGetFloatv(GL_MAX_VIEWPORT_DIMS, viewport_dims);
        milton_log("Maximum viewport dimensions, %fx%f\n", viewport_dims[0], viewport_dims[1]);
        render_data->viewport_limits[0] = viewport_dims[0];
        render_data->viewport_limits[1] = viewport_dims[1];
    }

    render_data->current_color = {-1,-1,-1,-1};
    render_data->current_radius = -1;

    GLCHK(glEnable(GL_SCISSOR_TEST));
    GLCHK(glActiveTexture(GL_TEXTURE0));
    bool result = true;

    // Create a single VAO and bind it.
    GLuint proxy_vao = 0;
    GLCHK( glGenVertexArrays(1, &proxy_vao) );
    GLCHK( glBindVertexArray(proxy_vao) );

    GLVendor vendor = GLVendor_UNKNOWN;
    {
        char *vendor_string = (char *)glGetString(GL_VENDOR);
        if ( vendor_string ) {
            if ( strcmp("NVIDIA Corporation", vendor_string) == 0 ) {
                vendor = GLVendor_NVIDIA;
            }
            else if ( strcmp("AMD", vendor_string) == 0 ) {
                vendor = GLVendor_AMD;
            }
            else if ( strcmp("Intel", vendor_string) == 0 ) {
                vendor = GLVendor_INTEL;
            }
        }
        milton_log("Vendor string: \"%s\"\n", vendor_string);
    }
    // Quad that fills the screen.
    {
        // a------d
        // |  \   |
        // |    \ |
        // b______c
        //  Triangle fan:
        GLfloat quad_data[] = {
            -1 , -1 , // a
            -1 , 1  , // b
            1  , 1  , // c
            1  , -1 , // d
        };

        // Create buffers and upload
        GLuint vbo = 0;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        DEBUG_gl_mark_buffer(vbo);
        glBufferData(GL_ARRAY_BUFFER, array_count(quad_data)*sizeof(*quad_data), quad_data, GL_STATIC_DRAW);

        float u = 1.0f;
        GLfloat uv_data[] = {
            0,0,
            0,u,
            u,u,
            u,0,
        };
        GLuint vbo_uv = 0;
        glGenBuffers(1, &vbo_uv);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_uv);
        DEBUG_gl_mark_buffer(vbo_uv);
        glBufferData(GL_ARRAY_BUFFER, array_count(uv_data)*sizeof(*uv_data), uv_data, GL_STATIC_DRAW);

        render_data->vbo_screen_quad = vbo;

        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_quad_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_quad_f, GL_FRAGMENT_SHADER);
        render_data->quad_program = glCreateProgram();
        gl_link_program(render_data->quad_program, objs, array_count(objs));
    }

    {  // Stroke raster program
        GLuint objs[2];

        char* config_string = "";
        if ( gl_helper_check_flags(GLHelperFlags_SAMPLE_SHADING) ) {
            if ( vendor == GLVendor_NVIDIA ) {
                config_string =
                        "#define HAS_SAMPLE_SHADING 1 \n"
                        "#define VENDOR_NVIDIA 1 \n";
            }
            else if ( vendor == GLVendor_INTEL ) {
                config_string =
                    "#define HAS_SAMPLE_SHADING 1 \n"
                    "#define VENDOR_INTEL 1 \n";
            }
            else {
                config_string =
                        "#define HAS_SAMPLE_SHADING 1 \n";
            }
        }

        objs[0] = gl_compile_shader(g_stroke_raster_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_stroke_raster_f, GL_FRAGMENT_SHADER, config_string);

        render_data->stroke_program = glCreateProgram();

        gl_link_program(render_data->stroke_program, objs, array_count(objs));

        GLCHK( glUseProgram(render_data->stroke_program) );
        gl_set_uniform_i(render_data->stroke_program, "u_canvas", 0);
    }
    {  // Color picker program
        render_data->picker_program = glCreateProgram();
        GLuint objs[2] = {};

        // g_picker_* generated by shadergen.cc
        objs[0] = gl_compile_shader(g_picker_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_picker_f, GL_FRAGMENT_SHADER);
        gl_link_program(render_data->picker_program, objs, array_count(objs));
        gl_set_uniform_i(render_data->picker_program, "u_canvas", 0);
    }
    {  // Layer blend program
        render_data->layer_blend_program = glCreateProgram();
        GLuint objs[2] = {};

        objs[0] = gl_compile_shader(g_layer_blend_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_layer_blend_f, GL_FRAGMENT_SHADER);
        gl_link_program(render_data->layer_blend_program, objs, array_count(objs));
        gl_set_uniform_i(render_data->layer_blend_program, "u_canvas", 0);
    }
    {  // Brush outline program
        render_data->outline_program = glCreateProgram();
        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_outline_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_outline_f, GL_FRAGMENT_SHADER);

        gl_link_program(render_data->outline_program, objs, array_count(objs));
    }
    {  // Exporter program
        render_data->exporter_program = glCreateProgram();

        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_exporter_rect_f, GL_FRAGMENT_SHADER);

        gl_link_program(render_data->exporter_program, objs, array_count(objs));
        gl_set_uniform_i(render_data->exporter_program, "u_canvas", 0);
    }
    {
        render_data->texture_fill_program = glCreateProgram();
        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_texture_fill_f, GL_FRAGMENT_SHADER);

        gl_link_program(render_data->texture_fill_program, objs, array_count(objs));
        gl_set_uniform_i(render_data->texture_fill_program, "u_canvas", 0);
    }
    {
        render_data->postproc_program = glCreateProgram();
        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_postproc_f, GL_FRAGMENT_SHADER);

        gl_link_program(render_data->postproc_program, objs, array_count(objs));
        gl_set_uniform_i(render_data->postproc_program, "u_canvas", 0);
    }
#if MILTON_DEBUG
    {  // Simple program
        render_data->simple_program = glCreateProgram();

        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_simple_f, GL_FRAGMENT_SHADER);

        gl_link_program(render_data->simple_program, objs, array_count(objs));
    }
#endif

    // Framebuffer object for canvas. Layer buffer
    {
        GLCHK (glGenTextures(1, &render_data->canvas_texture));

        if ( gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, render_data->canvas_texture);

            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                                    GL_RGBA,
                                    view->screen_size.w, view->screen_size.h,
                                    GL_TRUE);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, render_data->canvas_texture);
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR) );
            glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_RGBA8,
                         /*width, height = */ view->screen_size.w, view->screen_size.h,
                         /*border = */ 0,
                         /*format = */ GL_RGBA, /*type = */ GL_UNSIGNED_BYTE,
                         /*data = */ NULL);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        GLCHK (glGenTextures(1, &render_data->eraser_texture));
        if ( gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, render_data->eraser_texture);

            GLCHK( glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                                           GL_RGBA,
                                           view->screen_size.w, view->screen_size.h,
                                           GL_TRUE) );
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, render_data->eraser_texture);
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR) );

            glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_RGBA8,
                         /*width, height = */ view->screen_size.w, view->screen_size.h,
                         /*border = */ 0,
                         /*format = */ GL_RGBA, /*type = */ GL_UNSIGNED_BYTE,
                         /*data = */ NULL);

            glBindTexture(GL_TEXTURE_2D, 0);
        }

        GLCHK (glGenTextures(1, &render_data->helper_texture));

        if ( gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, render_data->helper_texture);

            GLCHK( glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                                           GL_RGBA,
                                           view->screen_size.w, view->screen_size.h,
                                           GL_TRUE) );
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, render_data->helper_texture);
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR) );

            glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_RGBA8,
                         /*width, height = */ view->screen_size.w, view->screen_size.h,
                         /*border = */ 0,
                         /*format = */ GL_RGBA, /*type = */ GL_UNSIGNED_BYTE,
                         /*data = */ NULL);

            glBindTexture(GL_TEXTURE_2D, 0);
        }


        glGenTextures(1, &render_data->stencil_texture);

        if ( gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {

            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, render_data->stencil_texture);

            GLCHK( glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                                           /*internalFormat, num of components*/GL_DEPTH24_STENCIL8,
                                           view->screen_size.w, view->screen_size.h,
                                           GL_TRUE) );


            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, render_data->stencil_texture);
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST) );
            GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST) );
            //GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY) );
            // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
            // GLCHK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL) );
            GLCHK( glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_DEPTH24_STENCIL8,
                                 /*width, height = */ view->screen_size.w, view->screen_size.h,
                                 /*border = */ 0,
                                 /*format = */ GL_DEPTH_STENCIL, /*type = */ GL_UNSIGNED_INT_24_8,
                                 /*data = */ NULL) );
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // Create framebuffer object.
        {
            GLuint fbo = 0;
            GLCHK(glGenFramebuffersEXT(1, &fbo));
            GLCHK(glBindFramebufferEXT(GL_FRAMEBUFFER, fbo));
            GLenum texture_target;
            if ( gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
                texture_target = GL_TEXTURE_2D_MULTISAMPLE;
            }
            else{
                texture_target = GL_TEXTURE_2D;
            }

            GLCHK( glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                                             render_data->canvas_texture, 0) );
            GLCHK( glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, texture_target,
                                             render_data->stencil_texture, 0) );
            render_data->fbo = fbo;
            print_framebuffer_status();
        }
        GLCHK( glBindFramebufferEXT(GL_FRAMEBUFFER, 0) );
    }
    // VBO for picker
    glGenBuffers(1, &render_data->vbo_picker);
    glGenBuffers(1, &render_data->vbo_picker_norm);

    // Call gpu_update_picker() to initialize the color picker
    gpu_update_picker(render_data, picker);
    return result;
}

void
gpu_resize(RenderData* render_data, CanvasView* view)
{
    render_data->width = view->screen_size.w;
    render_data->height = view->screen_size.h;

    if ( gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        GLCHK (glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, render_data->eraser_texture));
        GLCHK (glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                                       GL_RGBA,
                                       render_data->width, render_data->height,
                                       GL_TRUE));

        GLCHK (glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, render_data->canvas_texture));
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                                GL_RGBA,
                                render_data->width, render_data->height,
                                GL_TRUE);

        GLCHK (glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, render_data->stencil_texture));
        GLCHK (glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_NUM_SAMPLES,
                                       GL_DEPTH24_STENCIL8,
                                       render_data->width, render_data->height,
                                       GL_TRUE) );
    }
    else {
        glBindTexture(GL_TEXTURE_2D, render_data->eraser_texture);
        glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_RGBA8,
                     /*width, height = */ render_data->width, render_data->height,
                     /*border = */ 0,
                     /*format = */ GL_RGBA, /*type = */ GL_UNSIGNED_BYTE,
                     /*data = */ NULL);
        glBindTexture(GL_TEXTURE_2D, render_data->canvas_texture);
        glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_RGBA8,
                     /*width, height = */ render_data->width, render_data->height,
                     /*border = */ 0,
                     /*format = */ GL_RGBA, /*type = */ GL_UNSIGNED_BYTE,
                     /*data = */ NULL);
        glBindTexture(GL_TEXTURE_2D, render_data->stencil_texture);

        GLCHK( glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_DEPTH24_STENCIL8,
                            /*width, height = */ render_data->width, render_data->height,
                            /*border = */ 0,
                            /*format = */ GL_DEPTH_STENCIL, /*type = */ GL_UNSIGNED_INT_24_8,
                            /*data = */ NULL) );
    }
}

void
gpu_reset_render_flags(RenderData* render_data, int flags)
{
    render_data->flags = flags;
}

void
gpu_update_scale(RenderData* render_data, i32 scale)
{
    render_data->scale = scale;
    gl_set_uniform_i(render_data->stroke_program, "u_scale", scale);
}

void
gpu_update_export_rect(RenderData* render_data, Exporter* exporter)
{
    if ( render_data->vbo_exporter[0] == 0 ) {
        glGenBuffers(4, render_data->vbo_exporter);
    }

    i32 x = min(exporter->pivot.x, exporter->needle.x);
    i32 y = min(exporter->pivot.y, exporter->needle.y);
    i32 w = abs(exporter->pivot.x - exporter->needle.x);
    i32 h = abs(exporter->pivot.y - exporter->needle.y);

    // Normalize to [-1,1]^2
    float normalized_rect[] = {
        2*((GLfloat)    x/(render_data->width))-1, -(2*((GLfloat)y    /(render_data->height))-1),
        2*((GLfloat)    x/(render_data->width))-1, -(2*((GLfloat)(y+h)/(render_data->height))-1),
        2*((GLfloat)(x+w)/(render_data->width))-1, -(2*((GLfloat)(y+h)/(render_data->height))-1),
        2*((GLfloat)(x+w)/(render_data->width))-1, -(2*((GLfloat)y    /(render_data->height))-1),
    };

    float px = 2.0f;
    float line_length = px / render_data->height;
    float aspect = render_data->width / (float)render_data->height;

    float top[] = {
        normalized_rect[0], normalized_rect[1],
        normalized_rect[2], normalized_rect[1]+line_length,
        normalized_rect[4], normalized_rect[1]+line_length,
        normalized_rect[6], normalized_rect[1],
    };
    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_exporter[0]);
    DEBUG_gl_mark_buffer(render_data->vbo_exporter[0]);
    GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(top)*sizeof(*top), top, GL_DYNAMIC_DRAW) );

    float bottom[] = {
        normalized_rect[0], normalized_rect[3]-line_length,
        normalized_rect[2], normalized_rect[3],
        normalized_rect[4], normalized_rect[3],
        normalized_rect[6], normalized_rect[3]-line_length,
    };
    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_exporter[1]);
    DEBUG_gl_mark_buffer(render_data->vbo_exporter[1]);
    GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(bottom)*sizeof(*bottom), bottom, GL_DYNAMIC_DRAW) );

    line_length = px / (render_data->width);

    float right[] = {
        normalized_rect[4]-line_length, normalized_rect[1],
        normalized_rect[4]-line_length, normalized_rect[3],
        normalized_rect[4], normalized_rect[5],
        normalized_rect[4], normalized_rect[7],
    };
    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_exporter[2]);
    DEBUG_gl_mark_buffer(render_data->vbo_exporter[2]);
    GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(right)*sizeof(*right), right, GL_DYNAMIC_DRAW) );

    float left[] = {
        normalized_rect[0], normalized_rect[1],
        normalized_rect[0], normalized_rect[3],
        normalized_rect[0]+line_length, normalized_rect[5],
        normalized_rect[0]+line_length, normalized_rect[7],
    };
    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_exporter[3]);
    DEBUG_gl_mark_buffer(render_data->vbo_exporter[3]);
    GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(left)*sizeof(*left), left, GL_DYNAMIC_DRAW) );
}

void
gpu_update_background(RenderData* render_data, v3f background_color)
{
    render_data->background_color = background_color;
}

void
gpu_get_viewport_limits(RenderData* render_data, float* out_viewport_limits)
{
    if ( out_viewport_limits ) {
        out_viewport_limits[0] = render_data->viewport_limits[0];
        out_viewport_limits[1] = render_data->viewport_limits[1];
    }
}

i32
gpu_get_num_clipped_strokes(Layer* root_layer)
{
    i32 count = 0;
    #if MILTON_ENABLE_PROFILING
    for ( Layer* l = root_layer; l != NULL; l = l->next ) {
        StrokeList strokes = l->strokes;
        for ( i64 si = 0; si < strokes.count; ++si ) {
            Stroke* s = strokes[si];
            if ( s->render_element.vbo_stroke != 0 ) {
                ++count;
            }
        }
    }
    #endif
    return count;
}

static void
set_screen_size(RenderData* render_data, float* fscreen)
{
    GLuint programs[] = {
        render_data->stroke_program,
        render_data->layer_blend_program,
        render_data->texture_fill_program,
        render_data->exporter_program,
        render_data->picker_program,
        render_data->postproc_program,
    };
    for ( int pi = 0; pi < array_count(programs); ++pi ) {
        gl_set_uniform_vec2(programs[pi], "u_screen_size", 1, fscreen);
    }
}

void
gpu_update_canvas(RenderData* render_data, CanvasView* view)
{
    glUseProgram(render_data->stroke_program);

    auto center = view->zoom_center;
    auto pan = view->pan_vector;
    gl_set_uniform_vec2i(render_data->stroke_program, "u_pan_vector", 1, pan.d);
    gl_set_uniform_vec2i(render_data->stroke_program, "u_zoom_center", 1, center.d);
    gpu_update_scale(render_data, view->scale);
    float fscreen[] = { (float)view->screen_size.x, (float)view->screen_size.y };
    set_screen_size(render_data, fscreen);
}

void
gpu_cook_stroke(Arena* arena, RenderData* render_data, Stroke* stroke, CookStrokeOpt cook_option)
{
    render_data->stroke_z = (render_data->stroke_z + 1) % (MAX_DEPTH_VALUE-1);
    const i32 stroke_z = render_data->stroke_z + 1;

    if ( cook_option == CookStroke_NEW && stroke->render_element.vbo_stroke != 0 ) {
        // We already have our data cooked
        mlt_assert(stroke->render_element.vbo_pointa != 0);
        mlt_assert(stroke->render_element.vbo_pointb != 0);
    } else {
        vec2 cp;
        cp.x = stroke->points[stroke->num_points-1].x;
        cp.y = stroke->points[stroke->num_points-1].y;

        auto npoints = stroke->num_points;
        if ( npoints == 1 ) {
            // Create a 2-point stroke and recurse
            Stroke duplicate = *stroke;
            duplicate.num_points = 2;
            Arena scratch_arena = arena_push(arena);
            duplicate.points = arena_alloc_array(&scratch_arena, 2, v2i);
            duplicate.pressures = arena_alloc_array(&scratch_arena, 2, f32);
            duplicate.points[0] = stroke->points[0];
            duplicate.points[1] = stroke->points[0];
            duplicate.pressures[0] = stroke->pressures[0];
            duplicate.pressures[1] = stroke->pressures[0];

            gpu_cook_stroke(&scratch_arena, render_data, &duplicate, cook_option);

            // Copy render element to stroke
            stroke->render_element = duplicate.render_element;

            arena_pop(&scratch_arena);
        }
        else if ( npoints > 1 ) {
            GLCHK( glUseProgram(render_data->stroke_program) );

            // 3 (triangle) *
            // 2 (two per segment) *
            // N-1 (segments per stroke)
            // Reduced to 4 by using indices
            const size_t count_attribs = 4*((size_t)npoints-1);

            // 6 (3 * 2 from count_attribs)
            // N-1 (num segments)
            const size_t count_indices = 6*((size_t)npoints-1);

            v3f* bounds;
            v3f* apoints;
            v3f* bpoints;
            u16* indices;
            Arena scratch_arena = arena_push(arena,
                                             count_attribs*sizeof(decltype(*bounds))  // Bounds
                                             + 2*count_attribs*sizeof(decltype(*apoints)) // Attributes a,b
                                             + count_indices*sizeof(decltype(*indices)));  // Indices

            bounds  = arena_alloc_array(&scratch_arena, count_attribs, v3f);
            apoints = arena_alloc_array(&scratch_arena, count_attribs, v3f);
            bpoints = arena_alloc_array(&scratch_arena, count_attribs, v3f);
            indices = arena_alloc_array(&scratch_arena, count_indices, u16);

            mlt_assert(render_data->scale > 0);

            size_t bounds_i = 0;
            size_t apoints_i = 0;
            size_t bpoints_i = 0;
            size_t indices_i = 0;
            for ( i64 i=0; i < npoints-1; ++i ) {
                v2i point_i = stroke->points[i];
                v2i point_j = stroke->points[i+1];

                Brush brush = stroke->brush;
                float radius_i = stroke->pressures[i]*brush.radius;
                float radius_j = stroke->pressures[i+1]*brush.radius;

                i32 min_x = min(point_i.x-radius_i, point_j.x-radius_j);
                i32 min_y = min(point_i.y-radius_i, point_j.y-radius_j);

                i32 max_x = max(point_i.x+radius_i, point_j.x+radius_j);
                i32 max_y = max(point_i.y+radius_i, point_j.y+radius_j);

                // Bounding geometry and attributes

                mlt_assert (bounds_i < ((1<<16)-4));
                u16 idx = (u16)bounds_i;

                bounds[bounds_i++] = { (float)min_x, (float)min_y, (float)stroke_z };
                bounds[bounds_i++] = { (float)min_x, (float)max_y, (float)stroke_z };
                bounds[bounds_i++] = { (float)max_x, (float)max_y, (float)stroke_z };

                // Leaving this commented to show how each quad works, conceptually.
                //bounds[bounds_i++] = { (float)max_x, (float)max_y, (float)stroke_z };
                //bounds[bounds_i++] = { (float)min_x, (float)min_y, (float)stroke_z };
                bounds[bounds_i++]   = { (float)max_x, (float)min_y, (float)stroke_z };

                indices[indices_i++] = (u16)(idx + 0);
                indices[indices_i++] = (u16)(idx + 1);
                indices[indices_i++] = (u16)(idx + 2);

                //indices[indices_i++] = (u16)(idx + 3);
                indices[indices_i++] = (u16)(idx + 2);

                //indices[indices_i++] = (u16)(idx + 4);
                indices[indices_i++] = (u16)(idx + 0);

                //indices[indices_i++] = (u16)(idx + 5);
                indices[indices_i++] = (u16)(idx + 3);

                // Pressures are in (0,1] but we need to encode them as integers.
                float pressure_a = stroke->pressures[i];
                float pressure_b = stroke->pressures[i+1];

                // Add attributes for each new vertex.
                for ( int repeat = 0; repeat < 4; ++repeat ) {
                    apoints[apoints_i++] = { (float)point_i.x, (float)point_i.y, pressure_a };
                    bpoints[bpoints_i++] = { (float)point_j.x, (float)point_j.y, pressure_b };
                }
            }

            mlt_assert(bounds_i == count_attribs);
            mlt_assert(apoints_i == bpoints_i);
            mlt_assert(apoints_i == bounds_i);

            // TODO: check for GL_OUT_OF_MEMORY

            GLuint vbo_stroke = 0;
            GLuint vbo_pointa = 0;
            GLuint vbo_pointb = 0;
            GLuint indices_buffer = 0;

            GLenum hint = GL_STATIC_DRAW;
            if ( cook_option == CookStroke_UPDATE_WORKING_STROKE ) {
                hint = GL_DYNAMIC_DRAW;
            }
            // Cooking the stroke for the first time.
            if ( stroke->render_element.vbo_stroke == 0 ) {
                glGenBuffers(1, &vbo_stroke);
                glGenBuffers(1, &vbo_pointa);
                glGenBuffers(1, &vbo_pointb);
                glGenBuffers(1, &indices_buffer);

                DEBUG_gl_mark_buffer(vbo_stroke);
                DEBUG_gl_mark_buffer(vbo_pointa);
                DEBUG_gl_mark_buffer(vbo_pointb);
                DEBUG_gl_mark_buffer(indices_buffer);

                glBindBuffer(GL_ARRAY_BUFFER, vbo_stroke);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bounds))), bounds, hint) );
                glBindBuffer(GL_ARRAY_BUFFER, vbo_pointa);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*apoints))), apoints, hint) );
                glBindBuffer(GL_ARRAY_BUFFER, vbo_pointb);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bpoints))), bpoints, hint) );
                glBindBuffer(GL_ARRAY_BUFFER, indices_buffer);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(indices_i*sizeof(decltype(*indices))), indices, hint) );
            }
            // Updating the working stroke
            else {
                vbo_stroke = stroke->render_element.vbo_stroke;
                vbo_pointa = stroke->render_element.vbo_pointa;
                vbo_pointb = stroke->render_element.vbo_pointb;
                indices_buffer = stroke->render_element.indices;

                glBindBuffer(GL_ARRAY_BUFFER, vbo_stroke);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bounds))), NULL, hint) );
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bounds))), bounds, hint) );
                glBindBuffer(GL_ARRAY_BUFFER, vbo_pointa);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*apoints))), NULL, hint) );
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*apoints))), apoints, hint) );
                glBindBuffer(GL_ARRAY_BUFFER, vbo_pointb);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bpoints))), NULL, hint) );
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bpoints))), bpoints, hint) );
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buffer);
                GLCHK( glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices_i*sizeof(decltype(*indices))), NULL, hint) );
                GLCHK( glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices_i*sizeof(decltype(*indices))), indices, hint) );
            }

            GLuint ubo = 0;

            RenderElement re = stroke->render_element;
            re.vbo_stroke = vbo_stroke;
            re.vbo_pointa = vbo_pointa;
            re.vbo_pointb = vbo_pointb;
            re.indices = indices_buffer;
            re.count = (i64)(indices_i);
            re.color = { stroke->brush.color.r, stroke->brush.color.g, stroke->brush.color.b, stroke->brush.color.a };
            re.radius = stroke->brush.radius;
            mlt_assert(re.count > 1);

            stroke->render_element = re;

            arena_pop(&scratch_arena);
        }
    }
}

void
gpu_free_strokes(Stroke* strokes, i64 count, RenderData* render_data)
{
    glFlush();
    glFinish();
    for ( i64 i = 0; i < count; ++i ) {
        Stroke* s = &strokes[i];
        RenderElement* re = &s->render_element;
        if ( re->vbo_stroke != 0 ) {
            mlt_assert(re->vbo_pointa != 0);
            mlt_assert(re->vbo_pointb != 0);
            mlt_assert(re->indices != 0);

            DEBUG_gl_validate_buffer(re->vbo_stroke);
            DEBUG_gl_validate_buffer(re->vbo_pointa);
            DEBUG_gl_validate_buffer(re->vbo_pointb);
            DEBUG_gl_validate_buffer(re->indices);

            mlt_assert(re->vbo_stroke != render_data->vbo_screen_quad);
            mlt_assert(re->vbo_pointa != render_data->vbo_screen_quad);
            mlt_assert(re->vbo_pointb != render_data->vbo_screen_quad);
            mlt_assert(re->indices != render_data->vbo_screen_quad);


            glDeleteBuffers(1, &re->vbo_stroke);
            glDeleteBuffers(1, &re->vbo_pointa);
            glDeleteBuffers(1, &re->vbo_pointb);
            glDeleteBuffers(1, &re->indices);

            DEBUG_gl_unmark_buffer(re->vbo_stroke);
            DEBUG_gl_unmark_buffer(re->vbo_pointa);
            DEBUG_gl_unmark_buffer(re->vbo_pointb);
            DEBUG_gl_unmark_buffer(re->indices);

            re->vbo_stroke = 0;
            re->vbo_pointa = 0;
            re->vbo_pointb = 0;
            re->indices = 0;
        }
    }
    glFlush();
    glFinish();
}

void
gpu_free_strokes(MiltonState* milton_state)
{
    if ( milton_state->canvas->root_layer != NULL ) {
        for ( Layer* l = milton_state->canvas->root_layer;
              l != NULL;
              l = l->next ) {
            StrokeList* sl = &l->strokes;
            StrokeBucket* bucket = &sl->root;
            i64 count = sl->count;
            while ( bucket ) {
                i64 bucketcount = 0;
                if ( count >= STROKELIST_BUCKET_COUNT ) {
                    bucketcount = STROKELIST_BUCKET_COUNT;
                    count -= STROKELIST_BUCKET_COUNT * (count / STROKELIST_BUCKET_COUNT);
                    // count -= STROKELIST_BUCKET_COUNT;
                } else {
                    bucketcount = count;
                }
                gpu_free_strokes(bucket->data, count, milton_state->render_data);
                bucket = bucket->next;
            }
        }
    }
}

void
gpu_clip_strokes_and_update(Arena* arena,
                            RenderData* render_data,
                            CanvasView* view,
                            Layer* root_layer, Stroke* working_stroke,
                            i32 x, i32 y, i32 w, i32 h, ClipFlags flags)
{
    DArray<RenderElement>* clip_array = &render_data->clip_array;

    RenderElement layer_element = {};
    layer_element.flags |= RenderElementFlags_LAYER;

    Rect screen_bounds;
    screen_bounds.left = 0;
    screen_bounds.right = render_data->width;
    screen_bounds.top = render_data->height;
    screen_bounds.bottom = 0;

    screen_bounds.left = x;
    screen_bounds.right = x + w;
    screen_bounds.top = y;
    screen_bounds.bottom = y+h;

    reset(clip_array);
    #if MILTON_ENABLE_PROFILING
    {
        render_data->clipped_count = 0;
    }
    #endif
    for ( Layer* l = root_layer;
          l != NULL;
          l = l->next ) {
        if ( !(l->flags & LayerFlags_VISIBLE) ) {
            // Skip invisible layers.
            continue;
        }

        StrokeBucket* bucket = &l->strokes.root;
        i64 bucket_i = 0;

        while ( bucket ) {
            i64 count = 0;
            if ( l->strokes.count - bucket_i*STROKELIST_BUCKET_COUNT >= STROKELIST_BUCKET_COUNT ) {
                count = STROKELIST_BUCKET_COUNT;
            } else {
                count = l->strokes.count % STROKELIST_BUCKET_COUNT;
            }
            Rect bbox = bucket->bounding_rect;
            bbox.top_left = canvas_to_raster(view, bbox.top_left);
            bbox.bot_right = canvas_to_raster(view, bbox.bot_right);

            b32 bucket_outside =   screen_bounds.left   > bbox.right
                                || screen_bounds.top    > bbox.bottom
                                || screen_bounds.right  < bbox.left
                                || screen_bounds.bottom < bbox.top;

            if ( !bucket_outside ) {
                for ( i64 i = 0; i < count; ++i ) {
                    Stroke* s = &bucket->data[i];

                    if ( s != NULL ) {
                        Rect bounds = s->bounding_rect;
                        bounds.top_left = canvas_to_raster(view, bounds.top_left);
                        bounds.bot_right = canvas_to_raster(view, bounds.bot_right);

                        b32 is_outside = bounds.left > (x+w) || bounds.right < x
                                || bounds.top > (y+h) || bounds.bottom < y;

                        i32 area = (bounds.right-bounds.left) * (bounds.bottom-bounds.top);

                        if ( !is_outside && area!=0 ) {
                            gpu_cook_stroke(arena, render_data, s);
                            push(clip_array, s->render_element);
                        }
                        else if ( is_outside && ( flags & ClipFlags_UPDATE_GPU_DATA ) ) {
                            // If it is far away, delete.
                            i32 distance = abs(bounds.left - x + bounds.top - y);
                            const i32 min_number_of_screens = 4;
                            if (    bounds.top    < y - min_number_of_screens*h
                                 || bounds.bottom > y+h + min_number_of_screens*h
                                 || bounds.left   > x+w + min_number_of_screens*w
                                 || bounds.right  < x - min_number_of_screens*w ) {
                                gpu_free_strokes(s, 1, render_data);
                            }
                        }
                    }
                }
            }
            else if ( flags & ClipFlags_UPDATE_GPU_DATA ) {
                gpu_free_strokes(bucket->data, count, render_data);
            }
            #if MILTON_ENABLE_PROFILING
            {
                for ( i64 i = 0; i < count; ++i ) {
                    Stroke* s = &bucket->data[i];
                    if ( s->render_element.vbo_stroke != 0 ) {
                        render_data->clipped_count++;
                    }
                }
            }
            #endif
            bucket = bucket->next;
            bucket_i += 1;
        }


        // Add the working stroke on the current layer.
        if ( working_stroke->layer_id == l->id ) {
            if ( working_stroke->num_points > 0 ) {
                gpu_cook_stroke(arena, render_data, working_stroke, CookStroke_UPDATE_WORKING_STROKE);

                push(clip_array, working_stroke->render_element);
            }
        }

        push(clip_array, layer_element)->layer_alpha = l->alpha;
    }
}

static void
gpu_render_canvas(RenderData* render_data, i32 view_x, i32 view_y,
                  i32 view_width, i32 view_height, float background_alpha=1.0f)
{
    // FLip it. GL is bottom-left.
    i32 x = view_x;
    i32 y = render_data->height - (view_y+view_height);
    i32 w = view_width;
    i32 h = view_height;
    glScissor(x, y, w, h);

    GLCHK(glBindFramebufferEXT(GL_FRAMEBUFFER, render_data->fbo));

    GLenum texture_target;
    if ( gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        texture_target = GL_TEXTURE_2D_MULTISAMPLE;
    } else {
        texture_target = GL_TEXTURE_2D;
    }

    GLuint layer_texture = render_data->helper_texture;

    if ( background_alpha != 0.0f ) {
        // Not sure if this works OK with background_alpha != 1.0f
        glClearColor(render_data->background_color.r, render_data->background_color.g,
                     render_data->background_color.b, background_alpha);
    } else {
        glClearColor(0,0,0,0);
    }

    GLCHK(glBindTexture(texture_target, render_data->eraser_texture));

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                              render_data->eraser_texture, 0);

    glClear(GL_COLOR_BUFFER_BIT);

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                              render_data->canvas_texture, 0);

    glClear(GL_COLOR_BUFFER_BIT);

    GLCHK(glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                                    layer_texture, 0));
    glClearColor(0,0,0,0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_NOTEQUAL);

    GLCHK(glUseProgram(render_data->stroke_program));

    GLint loc = glGetAttribLocation(render_data->stroke_program, "a_position");
    GLint loc_a = glGetAttribLocation(render_data->stroke_program, "a_pointa");
    GLint loc_b = glGetAttribLocation(render_data->stroke_program, "a_pointb");
    if ( loc >= 0 ) {
        DArray<RenderElement>* clip_array = &render_data->clip_array;

        for ( i64 i = 0; i < (i64)clip_array->count; i++ ) {
            RenderElement* re = &clip_array->data[i];

            if ( re->flags & RenderElementFlags_LAYER ) {
                // Layer render element.
                // The current framebuffer's color attachment is layer_texture.

                // Process this layer

                // Blit layer_texture to canvas_texture
                {
                    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              texture_target, render_data->canvas_texture, 0);
                    glBindTexture(texture_target, layer_texture);

                    glDisable(GL_DEPTH_TEST);

                    glUseProgram(render_data->texture_fill_program);
                    gl_set_uniform_f(render_data->texture_fill_program, "u_alpha", re->layer_alpha);
                    {
                        GLint t_loc = glGetAttribLocation(render_data->texture_fill_program, "a_position");
                        if ( t_loc >= 0 ) {
                            glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_screen_quad);
                            glEnableVertexAttribArray((GLuint)t_loc);
                            GLCHK(glVertexAttribPointer(/*attrib location*/ (GLuint)t_loc,
                                                        /*size*/ 2, GL_FLOAT, /*normalize*/ GL_FALSE,
                                                        /*stride*/ 0, /*ptr*/ 0));
                            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                        }
                    }

                    glEnable(GL_DEPTH_TEST);
                }

                // Copy canvas_texture's contents to the eraser_texture.
                {
                    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              texture_target, render_data->eraser_texture, 0);
                    glBindTexture(texture_target, render_data->canvas_texture);
                    glClear(GL_COLOR_BUFFER_BIT);
                    glDisable(GL_BLEND);
                    glDisable(GL_DEPTH_TEST);

                    glUseProgram(render_data->texture_fill_program);
                    gl_set_uniform_f(render_data->texture_fill_program, "u_alpha", 0.0f);
                    {
                        GLint t_loc = glGetAttribLocation(render_data->texture_fill_program, "a_position");
                        if ( t_loc >= 0 ) {
                            glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_screen_quad);
                            glEnableVertexAttribArray((GLuint)t_loc);
                            GLCHK(glVertexAttribPointer(/*attrib location*/ (GLuint)t_loc,
                                                        /*size*/ 2, GL_FLOAT, /*normalize*/ GL_FALSE,
                                                        /*stride*/ 0, /*ptr*/ 0));
                            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                        }
                    }

                    // Clear the layer texture.
                    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              texture_target, layer_texture, 0);
                    glClearColor(0,0,0,0);
                    glClear(GL_COLOR_BUFFER_BIT);

                    glBindTexture(texture_target, render_data->eraser_texture);
                    glUseProgram(render_data->stroke_program);
                    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_BLEND);
                }

            }
            // If this element is not a layer, then it is a stroke.
            else {
                i64 count = re->count;

                if ( count > 0 ) {
                    if ( equ4f(render_data->current_color, re->color) == false ) {
                        gl_set_uniform_vec4(render_data->stroke_program, "u_brush_color", 1,
                                            re->color.d);
                        render_data->current_color = re->color;
                    }
                    if ( render_data->current_radius != re->radius ) {
                        gl_set_uniform_i(render_data->stroke_program, "u_radius", re->radius);
                        render_data->current_radius = re->radius;
                    }

                    DEBUG_gl_validate_buffer(re->vbo_stroke);
                    DEBUG_gl_validate_buffer(re->vbo_pointa);
                    DEBUG_gl_validate_buffer(re->vbo_pointb);
                    DEBUG_gl_validate_buffer(re->indices);

                    if ( loc_a >= 0 ) {
                        GLCHK(glBindBuffer(GL_ARRAY_BUFFER, re->vbo_pointa));
                        GLCHK(glEnableVertexAttribArray((GLuint)loc_a));
                        GLCHK(glVertexAttribPointer(/*attrib location*/ (GLuint)loc_a,
                                                    /*size*/ 3, GL_FLOAT, /*normalize*/ GL_FALSE,
                                                    /*stride*/ 0, /*ptr*/ 0));
                    }
                    if ( loc_b >= 0 ) {
                        GLCHK(glBindBuffer(GL_ARRAY_BUFFER, re->vbo_pointb));
                        GLCHK(glEnableVertexAttribArray((GLuint)loc_b));
                        GLCHK(glVertexAttribPointer(/*attrib location*/ (GLuint)loc_b,
                                                    /*size*/ 3, GL_FLOAT, /*normalize*/ GL_FALSE,
                                                    /*stride*/ 0, /*ptr*/ 0));
                    }

                    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, re->vbo_stroke));
                    GLCHK(glEnableVertexAttribArray((GLuint)loc));
                    GLCHK(glVertexAttribPointer(/*attrib location*/ (GLuint)loc,
                                                /*size*/ 3, GL_FLOAT, /*normalize*/ GL_FALSE,
                                                /*stride*/ 0, /*ptr*/ 0));

                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, re->indices);

                    // Disable blending if this element is an eraser brush stroke.
                    if ( is_eraser(re->color) ) {
                        glDisable(GL_BLEND);
                        GLCHK(glBindTexture(texture_target, render_data->eraser_texture));
                        gl_set_uniform_i(render_data->stroke_program, "u_canvas", 0);
                    }

                    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, 0);

                    if ( is_eraser(re->color) ) {
                        glEnable(GL_BLEND);
                    }

                } else {
                    static int n = 0;
                    milton_log("Warning: Render element with count 0 [%d times]\n", ++  n);
                }
            }
        }
    }
    glViewport(0, 0, render_data->width, render_data->height);
    GLCHK(glScissor(0, 0, render_data->width, render_data->height));
}

void
gpu_render(RenderData* render_data,  i32 view_x, i32 view_y, i32 view_width, i32 view_height)
{
    glClearDepth(0.0f);
    glViewport(0, 0, render_data->width, render_data->height);
    glScissor(0, 0, render_data->width, render_data->height);
    glEnable(GL_BLEND);

    print_framebuffer_status();

    gpu_render_canvas(render_data, view_x, view_y, view_width, view_height);

    // Fill screen with canvas_texture

    GLenum texture_target;
    if ( gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        texture_target = GL_TEXTURE_2D_MULTISAMPLE;
    } else {
        texture_target = GL_TEXTURE_2D;
    }

    glBindTexture(texture_target, render_data->canvas_texture);
    GLCHK( glBindFramebufferEXT(GL_FRAMEBUFFER, 0) );
    glDisable(GL_DEPTH_TEST);

    #if 0
    glUseProgram(render_data->texture_fill_program);
    {
        GLint loc = glGetAttribLocation(render_data->texture_fill_program, "a_position");
        if ( loc >= 0 ) {
            glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_screen_quad);
            glEnableVertexAttribArray((GLuint)loc);
            GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                         /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                         /*stride*/0, /*ptr*/0));
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }
    #else
    {
        GLCHK( glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0) );
        GLCHK( glBindFramebufferEXT(GL_READ_FRAMEBUFFER, render_data->fbo) );
        GLCHK( glBlitFramebufferEXT(0, 0, render_data->width, render_data->height,
                                    0, 0, render_data->width, render_data->height, GL_COLOR_BUFFER_BIT, GL_NEAREST) );
    }
    #endif

    // Render Gui

    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);

    // Do post-processing of the rendered painting. Do it after rendering GUI elements
    // gets anti-aliased too.
    if ( !gl_helper_check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        GLCHK( glBindFramebufferEXT(GL_FRAMEBUFFER, 0) );

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, render_data->canvas_texture);

        gl_set_uniform_i(render_data->postproc_program, "u_canvas", 0);

        glUseProgram(render_data->postproc_program);

        GLint loc = glGetAttribLocation(render_data->postproc_program, "a_position");
        if ( loc >= 0 ) {
            DEBUG_gl_validate_buffer(render_data->vbo_screen_quad);
            glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_screen_quad);
            glVertexAttribPointer((GLuint)loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray((GLuint)loc);
            GLCHK(glVertexAttribPointer(/*attrib location*/ (GLuint)loc,
                                        /*size*/ 2, GL_FLOAT, /*normalize*/ GL_FALSE,
                                        /*stride*/ 0, /*ptr*/ 0));

            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }

    // TODO: Only render if view rect intersects picker rect
    if ( render_data->flags & RenderDataFlags_GUI_VISIBLE ) {
        // Render picker
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(render_data->picker_program);
        GLint loc = glGetAttribLocation(render_data->picker_program, "a_position");

        if ( loc >= 0 ) {
            DEBUG_gl_validate_buffer(render_data->vbo_picker);
            GLCHK( glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_picker) );
            GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                         /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                         /*stride*/0, /*ptr*/0));
            glEnableVertexAttribArray((GLuint)loc);
            GLint loc_norm = glGetAttribLocation(render_data->picker_program, "a_norm");
            if ( loc_norm >= 0 ) {
                DEBUG_gl_validate_buffer(render_data->vbo_picker_norm);
                GLCHK( glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_picker_norm) );
                GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc_norm,
                                             /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                             /*stride*/0, /*ptr*/0));
                glEnableVertexAttribArray((GLuint)loc_norm);

            }
            GLCHK( glDrawArrays(GL_TRIANGLE_FAN,0,4) );
        }
    }

    // Render outline
    {
        glUseProgram(render_data->outline_program);
        GLint loc = glGetAttribLocation(render_data->outline_program, "a_position");
        if ( loc >= 0 ) {
            DEBUG_gl_validate_buffer(render_data->vbo_outline);
            glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_outline);

            GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                         /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                         /*stride*/0, /*ptr*/0));
            glEnableVertexAttribArray((GLuint)loc);
            GLint loc_s = glGetAttribLocation(render_data->outline_program, "a_sizes");
            if ( loc_s >= 0 ) {
                DEBUG_gl_validate_buffer(render_data->vbo_outline_sizes);
                glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_outline_sizes);
                GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc_s,
                                             /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                             /*stride*/0, /*ptr*/0));
                glEnableVertexAttribArray((GLuint)loc_s);
            }
        }
        glDrawArrays(GL_TRIANGLE_FAN, 0,4);
    }
    glDisable(GL_BLEND);
    if ( render_data->flags & RenderDataFlags_EXPORTING ) {
        // Update data if rect is not degenerate.
        // Draw outline.
        glUseProgram(render_data->exporter_program);
        GLint loc = glGetAttribLocation(render_data->exporter_program, "a_position");
        if ( loc>=0 && render_data->vbo_exporter>0 ) {
            for ( int vbo_i = 0; vbo_i < 4; ++vbo_i ) {
                DEBUG_gl_validate_buffer(render_data->vbo_exporter[vbo_i]);
                glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_exporter[vbo_i]);
                glVertexAttribPointer((GLuint)loc, 2, GL_FLOAT, GL_FALSE, 0,0);
                glEnableVertexAttribArray((GLuint)loc);

                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
        }
    }

    GLCHK(glUseProgram(0));
}

void
gpu_render_to_buffer(MiltonState* milton_state, u8* buffer, i32 scale, i32 x, i32 y,
                     i32 w, i32 h, f32 background_alpha)
{
    CanvasView saved_view = *milton_state->view;
    RenderData* render_data = milton_state->render_data;
    CanvasView* view = milton_state->view;

    i32 saved_width = render_data->width;
    i32 saved_height = render_data->height;
    GLuint saved_fbo = render_data->fbo;

    i32 buf_w = w * scale;
    i32 buf_h = h * scale;

    v2i center = divide2i(milton_state->view->screen_size, 2);
    v2i pan_delta = sub2i(center, v2i{x + (w / 2), y + (h / 2)});

    milton_set_zoom_at_point(milton_state, center);

    milton_state->view->pan_vector =
        add2i(milton_state->view->pan_vector, scale2i(pan_delta, milton_state->view->scale));

    milton_state->view->screen_size = v2i{buf_w, buf_h};
    render_data->width = buf_w;
    render_data->height = buf_h;

    milton_state->view->zoom_center = divide2i(milton_state->view->screen_size, 2);
    if ( scale > 1 ) {
        milton_state->view->scale = (i32)ceill(((f32)milton_state->view->scale / (f32)scale));
    }

    gpu_resize(render_data, view);
    gpu_update_canvas(render_data, view);

    GLuint export_fbo = 0;
    GLuint export_texture = 0;  // The resized canvas.
    GLuint image_texture = 0;  // A copy of the texture, without multisampling.

    glGenTextures(1, &export_texture);

    glBindTexture(GL_TEXTURE_2D, export_texture);

    glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_RGBA8,
                 /*width, height = */ buf_w, buf_h, /*border = */ 0,
                 /*format = */ GL_RGBA, /*type = */ GL_UNSIGNED_BYTE,
                 /*data = */ NULL);



    GLCHK(glGenTextures(1, &image_texture));

    glBindTexture(GL_TEXTURE_2D, image_texture);

    glTexImage2D(GL_TEXTURE_2D, /*level = */ 0, /*internal_format = */ GL_RGBA8,
                 /*width, height = */ buf_w, buf_h, /*border = */ 0,
                 /*format = */ GL_RGBA, /*type = */ GL_UNSIGNED_BYTE,
                 /*data = */ NULL);

    {
        glGenFramebuffersEXT(1, &export_fbo);
        glBindFramebufferEXT(GL_FRAMEBUFFER, export_fbo);
        GLCHK(glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        GL_TEXTURE_2D, image_texture, 0));

        print_framebuffer_status();
    }

    // TODO: Check for out-of-memory errors.

    mlt_assert(buf_w == render_data->width);
    mlt_assert(buf_h == render_data->height);

    glViewport(0, 0, buf_w, buf_h);
    glScissor(0, 0, buf_w, buf_h);
    gpu_clip_strokes_and_update(&milton_state->root_arena, render_data, milton_state->view, milton_state->canvas->root_layer,
                                &milton_state->working_stroke, 0, 0, buf_w, buf_h);

    GLCHK( glBindFramebufferEXT(GL_FRAMEBUFFER, render_data->fbo) );

    gpu_render_canvas(render_data, 0, 0, buf_w, buf_h, background_alpha);


    // Post processing

    GLCHK( glBindFramebufferEXT(GL_FRAMEBUFFER, export_fbo) );

    glUseProgram(render_data->postproc_program);
    glBindTexture(GL_TEXTURE_2D, render_data->canvas_texture);

    GLint loc = glGetAttribLocation(render_data->postproc_program, "a_position");
    if ( loc >= 0 ) {
        DEBUG_gl_validate_buffer(render_data->vbo_screen_quad);
        glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_screen_quad);
        glVertexAttribPointer((GLuint)loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray((GLuint)loc);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    glEnable(GL_DEPTH_TEST);

    glBindFramebufferEXT(GL_FRAMEBUFFER, export_fbo);

    // Read onto buffer
    GLCHK( glReadPixels(0,0,
                        buf_w, buf_h,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        (GLvoid*)buffer) );

    {  // Flip texture
        u32* pixels = (u32*)buffer;
        for ( i64 j = 0; j < buf_h / 2; ++j ) {
            for ( i64 i = 0; i < buf_w; ++i ) {
                i64 idx_up = j * buf_w + i;
                i64 j_down = buf_h - 1 - j;
                i64 idx_down = j_down * buf_w + i;
                // Swap
                u32 pixel = pixels[idx_down];
                pixels[idx_down] = pixels[idx_up];
                pixels[idx_up] = pixel;
            }
        }
    }

    // Cleanup.

    render_data->fbo = saved_fbo;
    *milton_state->view = saved_view;
    render_data->width = saved_width;
    render_data->height = saved_height;

    glDeleteFramebuffersEXT(1, &export_fbo);
    glDeleteTextures(1, &export_texture);
    glBindFramebufferEXT(GL_FRAMEBUFFER, render_data->fbo);

    gpu_resize(render_data, view);
    gpu_update_canvas(render_data, view);

    // Re-render
    gpu_clip_strokes_and_update(&milton_state->root_arena,
                                render_data, milton_state->view, milton_state->canvas->root_layer,
                                &milton_state->working_stroke, 0, 0, render_data->width,
                                render_data->height);
    gpu_render(render_data, 0, 0, render_data->width, render_data->height);
}
void
gpu_release_data(RenderData* render_data)
{
    release(&render_data->clip_array);
}
