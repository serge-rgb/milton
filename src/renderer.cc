// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "shaders.gen.h"

#include "color.h"
#include "gl_helpers.h"
#include "gui.h"
#include "milton.h"
#include "vector.h"

#define MAX_DEPTH_VALUE (1<<20)     // Strokes have MAX_DEPTH_VALUE different z values. 1/i for each i in [1, MAX_DEPTH_VALUE)
                                    // Also defined in stroke_raster.v.glsl
                                    //
                                    // NOTE: Using this technique means that the algorithm is not correct.
                                    //  There is a low probability that one stroke will cover another
                                    //  stroke with the same z value.


#define RENDER_CHUNK_SIZE_LOG2 28

struct RenderData
{
    f32 viewport_limits[2];  // OpenGL limits to the framebuffer size.

    v2i render_center;

    // OpenGL programs.
    GLuint stroke_program;
#if STROKE_DEBUG_VIZ
    GLuint stroke_debug_program;
#endif
    GLuint quad_program;
    GLuint picker_program;
    GLuint layer_blend_program;
    GLuint outline_program;
    GLuint exporter_program;
    GLuint texture_fill_program;
    GLuint postproc_program;
    GLuint blur_program;
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
    i32 scale;  // zoom

    // See MAX_DEPTH_VALUE
    i32 stroke_z;

    // Cached values for stroke rendering uniforms.
    v4f current_color;
    float current_radius;

#if MILTON_ENABLE_PROFILING
    u64 clipped_count;
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
gpu_update_picker(RenderData* r, ColorPicker* picker)
{
    glUseProgram(r->picker_program);
    // Transform to [-1,1]
    v2f a = picker->data.a;
    v2f b = picker->data.b;
    v2f c = picker->data.c;
    Rect bounds = picker_get_bounds(picker);
    int w = bounds.right-bounds.left;
    int h = bounds.bottom-bounds.top;
    // The center of the picker has an offset of (25,35)
    // and the bounds radius is 100 px
    auto transform = [&](v2f p) { return v2f{2*p.x/w-1 - .25f, 2*p.y/h-1 -0.35f}; };
    a = transform(a);
    b = transform(b);
    c = transform(c);
    gl::set_uniform_vec2(r->picker_program, "u_pointa", 1, a.d);
    gl::set_uniform_vec2(r->picker_program, "u_pointb", 1, b.d);
    gl::set_uniform_vec2(r->picker_program, "u_pointc", 1, c.d);
    gl::set_uniform_f(r->picker_program, "u_angle", picker->data.hsv.h);

    v3f hsv = picker->data.hsv;
    gl::set_uniform_vec3(r->picker_program, "u_color", 1, hsv_to_rgb(hsv).d);

    // Point within triangle
    {
        v2f point = lerp(picker->data.b, lerp(picker->data.a, picker->data.c, hsv.s), hsv.v);
        // Move to [-1,1]^2
        point = transform(point);
        gl::set_uniform_vec2(r->picker_program, "u_triangle_point", 1, point.d);
    }
    v4f colors[5] = {};
    ColorButton* button = picker->color_buttons;
    colors[0] = button->rgba; button = button->next;
    colors[1] = button->rgba; button = button->next;
    colors[2] = button->rgba; button = button->next;
    colors[3] = button->rgba; button = button->next;
    colors[4] = button->rgba; button = button->next;
    gl::set_uniform_vec4(r->picker_program, "u_colors", 5, (float*)colors);

    // Update VBO for picker
    {
        Rect rect = get_bounds_for_picker_and_colors(picker);
        // convert to clip space
        v2i screen_size = {r->width, r->height};
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
        glBindBuffer(GL_ARRAY_BUFFER, r->vbo_picker);
        DEBUG_gl_mark_buffer(r->vbo_picker);
        glBufferData(GL_ARRAY_BUFFER, array_count(data)*sizeof(*data), data, GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, r->vbo_picker_norm);
        DEBUG_gl_mark_buffer(r->vbo_picker_norm);
        glBufferData(GL_ARRAY_BUFFER, array_count(norm)*sizeof(*norm), norm, GL_STATIC_DRAW);
    }
}

void
gpu_update_brush_outline(RenderData* r, i32 cx, i32 cy, i32 radius,
                         BrushOutlineEnum outline_enum, v4f color)
{
    if ( r->vbo_outline == 0 ) {
        mlt_assert(r->vbo_outline_sizes == 0);
        glGenBuffers(1, &r->vbo_outline);
        glGenBuffers(1, &r->vbo_outline_sizes);
    }
    mlt_assert(r->vbo_outline_sizes != 0);

    float radius_plus_girth = radius + 4.0f; // Girth defined in outline.f.glsl

    auto w = r->width;
    auto h = r->height;

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

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_outline_sizes);
    DEBUG_gl_mark_buffer(r->vbo_outline_sizes);
    glBufferData(GL_ARRAY_BUFFER, array_count(sizes)*sizeof(*sizes), sizes, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_outline);
    DEBUG_gl_mark_buffer(r->vbo_outline);
    glBufferData(GL_ARRAY_BUFFER, array_count(data)*sizeof(*data), data, GL_DYNAMIC_DRAW);

    gl::set_uniform_i(r->outline_program, "u_radius", radius);
    if ( outline_enum == BrushOutline_FILL ) {
        gl::set_uniform_i(r->outline_program, "u_fill", true);
        gl::set_uniform_vec4(r->outline_program, "u_color", 1, color.d);
    }
    else if ( outline_enum == BrushOutline_NO_FILL ) {
        gl::set_uniform_i(r->outline_program, "u_fill", false);
    }
}

b32
gpu_init(RenderData* r, CanvasView* view, ColorPicker* picker)
{
    r->stroke_z = MAX_DEPTH_VALUE - 20;

#if MULTISAMPLING_ENABLED
    if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        glEnable(GL_MULTISAMPLE);
        if ( gl::check_flags(GLHelperFlags_SAMPLE_SHADING) ) {
            glEnable(GL_SAMPLE_SHADING_ARB);
            glMinSampleShadingARB(1.0f);
        }
    }
#endif

    {
        GLfloat viewport_dims[2] = {};
        glGetFloatv(GL_MAX_VIEWPORT_DIMS, viewport_dims);
        milton_log("Maximum viewport dimensions, %fx%f\n", viewport_dims[0], viewport_dims[1]);
        r->viewport_limits[0] = viewport_dims[0];
        r->viewport_limits[1] = viewport_dims[1];
    }

    r->current_color = {-1,-1,-1,-1};
    r->current_radius = -1;

    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);
    bool result = true;

    // Create a single VAO and bind it.
    #if USE_GL_3_2
        GLuint proxy_vao = 0;
        glGenVertexArrays(1, &proxy_vao);
        glBindVertexArray(proxy_vao);
    #endif

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

        r->vbo_screen_quad = vbo;

        GLuint objs[2] = {};
        objs[0] = gl::compile_shader(g_quad_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_quad_f, GL_FRAGMENT_SHADER);
        r->quad_program = glCreateProgram();
        gl::link_program(r->quad_program, objs, array_count(objs));
    }

    {  // Stroke raster program
        GLuint objs[2];

        char* config_string = "";
        if ( gl::check_flags(GLHelperFlags_SAMPLE_SHADING) ) {
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

        objs[0] = gl::compile_shader(g_stroke_raster_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_stroke_raster_f, GL_FRAGMENT_SHADER, config_string);

        r->stroke_program = glCreateProgram();

        gl::link_program(r->stroke_program, objs, array_count(objs));

        gl::set_uniform_i(r->stroke_program, "u_canvas", 0);
    }
#if STROKE_DEBUG_VIZ
    {  // Stroke debug program
        r->stroke_debug_program = glCreateProgram();
        GLuint objs[2] = {};

        objs[0] = gl::compile_shader(g_stroke_raster_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_stroke_debug_f, GL_FRAGMENT_SHADER);

        r->stroke_debug_program = glCreateProgram();

        gl::link_program(r->stroke_debug_program, objs, array_count(objs));

        gl::set_uniform_i(r->stroke_debug_program, "u_canvas", 0);
    }
#endif
    {  // Color picker program
        r->picker_program = glCreateProgram();
        GLuint objs[2] = {};

        // g_picker_* generated by shadergen.cc
        objs[0] = gl::compile_shader(g_picker_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_picker_f, GL_FRAGMENT_SHADER);
        gl::link_program(r->picker_program, objs, array_count(objs));
        gl::set_uniform_i(r->picker_program, "u_canvas", 0);
    }
    {  // Layer blend program
        r->layer_blend_program = glCreateProgram();
        GLuint objs[2] = {};

        objs[0] = gl::compile_shader(g_layer_blend_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_layer_blend_f, GL_FRAGMENT_SHADER);
        gl::link_program(r->layer_blend_program, objs, array_count(objs));
        gl::set_uniform_i(r->layer_blend_program, "u_canvas", 0);
    }
    {  // Brush outline program
        r->outline_program = glCreateProgram();
        GLuint objs[2] = {};
        objs[0] = gl::compile_shader(g_outline_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_outline_f, GL_FRAGMENT_SHADER);

        gl::link_program(r->outline_program, objs, array_count(objs));
    }
    {  // Exporter program
        r->exporter_program = glCreateProgram();

        GLuint objs[2] = {};
        objs[0] = gl::compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_exporter_rect_f, GL_FRAGMENT_SHADER);

        gl::link_program(r->exporter_program, objs, array_count(objs));
        gl::set_uniform_i(r->exporter_program, "u_canvas", 0);
    }
    {
        r->texture_fill_program = glCreateProgram();
        GLuint objs[2] = {};
        objs[0] = gl::compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_texture_fill_f, GL_FRAGMENT_SHADER);

        gl::link_program(r->texture_fill_program, objs, array_count(objs));
        gl::set_uniform_i(r->texture_fill_program, "u_canvas", 0);
    }
    {
        r->postproc_program = glCreateProgram();
        GLuint objs[2] = {};
        objs[0] = gl::compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_postproc_f, GL_FRAGMENT_SHADER);

        gl::link_program(r->postproc_program, objs, array_count(objs));
        gl::set_uniform_i(r->postproc_program, "u_canvas", 0);
    }
    {
        r->blur_program = glCreateProgram();
        GLuint objs[2] = {};
        objs[0] = gl::compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_blur_f, GL_FRAGMENT_SHADER);
        gl::link_program(r->blur_program, objs, array_count(objs));
        gl::set_uniform_i(r->blur_program, "u_canvas", 0);
    }
#if MILTON_DEBUG
    {  // Simple program
        r->simple_program = glCreateProgram();

        GLuint objs[2] = {};
        objs[0] = gl::compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl::compile_shader(g_simple_f, GL_FRAGMENT_SHADER);

        gl::link_program(r->simple_program, objs, array_count(objs));
    }
#endif

    // Framebuffer object for canvas. Layer buffer
    {
#if MULTISAMPLING_ENABLED
        if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            r->canvas_texture = gl::new_color_texture_multisample(view->screen_size.w, view->screen_size.h);
        } else
#endif
        {
            r->canvas_texture = gl::new_color_texture(view->screen_size.w, view->screen_size.h);
        }

#if MULTISAMPLING_ENABLED
        if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            r->eraser_texture = gl::new_color_texture_multisample(view->screen_size.w, view->screen_size.h);
        } else
#endif
        {
            r->eraser_texture = gl::new_color_texture(view->screen_size.w, view->screen_size.h);
        }

        glGenTextures(1, &r->helper_texture);

#if MULTISAMPLING_ENABLED
        if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            r->helper_texture = gl::new_color_texture_multisample(view->screen_size.w, view->screen_size.h);
        } else
#endif
        {
            r->helper_texture = gl::new_color_texture(view->screen_size.w, view->screen_size.h);
        }


        glGenTextures(1, &r->stencil_texture);

#if MULTISAMPLING_ENABLED
        if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            r->stencil_texture = gl::new_depth_stencil_texture_multisample(view->screen_size.w, view->screen_size.h);
        }
        else
#endif
        {
            r->stencil_texture = gl::new_depth_stencil_texture(view->screen_size.w, view->screen_size.h);
        }

        // Create framebuffer object.
        GLenum texture_target;
#if MULTISAMPLING_ENABLED
        if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            texture_target = GL_TEXTURE_2D_MULTISAMPLE;
        }
        else
#endif
        {
            texture_target = GL_TEXTURE_2D;
        }
        r->fbo = gl::new_fbo(r->canvas_texture, r->stencil_texture, texture_target);
        glBindFramebufferEXT(GL_FRAMEBUFFER, r->fbo);
        print_framebuffer_status();
        glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
    }
    // VBO for picker
    glGenBuffers(1, &r->vbo_picker);
    glGenBuffers(1, &r->vbo_picker_norm);

    // Call gpu_update_picker() to initialize the color picker
    gpu_update_picker(r, picker);
    return result;
}

void
gpu_resize(RenderData* r, CanvasView* view)
{
    r->width = view->screen_size.w;
    r->height = view->screen_size.h;

#if MULTISAMPLING_ENABLED
    if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        gl::resize_color_texture_multisample(r->eraser_texture, r->width, r->height);
        gl::resize_color_texture_multisample(r->canvas_texture, r->width, r->height);
        gl::resize_color_texture_multisample(r->helper_texture, r->width, r->height);
        gl::resize_depth_stencil_texture_multisample(r->stencil_texture, r->width, r->height);
    }
    else
#endif
    {
        gl::resize_color_texture(r->eraser_texture, r->width, r->height);
        gl::resize_color_texture(r->canvas_texture, r->width, r->height);
        gl::resize_color_texture(r->helper_texture, r->width, r->height);
        gl::resize_depth_stencil_texture(r->stencil_texture, r->width, r->height);
    }
}

void
gpu_reset_render_flags(RenderData* r, int flags)
{
    r->flags = flags;
}

void
gpu_update_scale(RenderData* r, i32 scale)
{
    r->scale = scale;
    gl::set_uniform_i(r->stroke_program, "u_scale", scale);
    #if STROKE_DEBUG_VIZ
        gl::set_uniform_i(r->stroke_debug_program, "u_scale", scale);
    #endif
}

void
gpu_update_export_rect(RenderData* r, Exporter* exporter)
{
    if ( r->vbo_exporter[0] == 0 ) {
        glGenBuffers(4, r->vbo_exporter);
    }

    i32 x = min(exporter->pivot.x, exporter->needle.x);
    i32 y = min(exporter->pivot.y, exporter->needle.y);
    i32 w = MLT_ABS(exporter->pivot.x - exporter->needle.x);
    i32 h = MLT_ABS(exporter->pivot.y - exporter->needle.y);

    // Normalize to [-1,1]^2
    float normalized_rect[] = {
        2*((GLfloat)    x/(r->width))-1, -(2*((GLfloat)y    /(r->height))-1),
        2*((GLfloat)    x/(r->width))-1, -(2*((GLfloat)(y+h)/(r->height))-1),
        2*((GLfloat)(x+w)/(r->width))-1, -(2*((GLfloat)(y+h)/(r->height))-1),
        2*((GLfloat)(x+w)/(r->width))-1, -(2*((GLfloat)y    /(r->height))-1),
    };

    float px = 2.0f;
    float line_length = px / r->height;

    float top[] = {
        normalized_rect[0], normalized_rect[1],
        normalized_rect[2], normalized_rect[1]+line_length,
        normalized_rect[4], normalized_rect[1]+line_length,
        normalized_rect[6], normalized_rect[1],
    };
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_exporter[0]);
    DEBUG_gl_mark_buffer(r->vbo_exporter[0]);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(top)*sizeof(*top), top, GL_DYNAMIC_DRAW);

    float bottom[] = {
        normalized_rect[0], normalized_rect[3]-line_length,
        normalized_rect[2], normalized_rect[3],
        normalized_rect[4], normalized_rect[3],
        normalized_rect[6], normalized_rect[3]-line_length,
    };
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_exporter[1]);
    DEBUG_gl_mark_buffer(r->vbo_exporter[1]);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(bottom)*sizeof(*bottom), bottom, GL_DYNAMIC_DRAW);

    line_length = px / (r->width);

    float right[] = {
        normalized_rect[4]-line_length, normalized_rect[1],
        normalized_rect[4]-line_length, normalized_rect[3],
        normalized_rect[4], normalized_rect[5],
        normalized_rect[4], normalized_rect[7],
    };
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_exporter[2]);
    DEBUG_gl_mark_buffer(r->vbo_exporter[2]);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(right)*sizeof(*right), right, GL_DYNAMIC_DRAW);

    float left[] = {
        normalized_rect[0], normalized_rect[1],
        normalized_rect[0], normalized_rect[3],
        normalized_rect[0]+line_length, normalized_rect[5],
        normalized_rect[0]+line_length, normalized_rect[7],
    };
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_exporter[3]);
    DEBUG_gl_mark_buffer(r->vbo_exporter[3]);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(left)*sizeof(*left), left, GL_DYNAMIC_DRAW);
}

void
gpu_update_background(RenderData* r, v3f background_color)
{
    r->background_color = background_color;
}

void
gpu_get_viewport_limits(RenderData* r, float* out_viewport_limits)
{
    if ( out_viewport_limits ) {
        out_viewport_limits[0] = r->viewport_limits[0];
        out_viewport_limits[1] = r->viewport_limits[1];
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
set_screen_size(RenderData* r, float* fscreen)
{
    GLuint programs[] = {
        r->stroke_program,
        #if STROKE_DEBUG_VIZ
            r->stroke_debug_program,
        #endif
        r->layer_blend_program,
        r->texture_fill_program,
        r->exporter_program,
        r->picker_program,
        r->postproc_program,
        r->blur_program,
    };
    for ( u64 pi = 0; pi < array_count(programs); ++pi ) {
        gl::set_uniform_vec2(programs[pi], "u_screen_size", 1, fscreen);
    }
}

static
v2i
relative_to_render_center(RenderData* r, v2l point)
{
    v2i result = VEC2I(point - VEC2L(r->render_center*(1<<RENDER_CHUNK_SIZE_LOG2)));
    return result;
}

void
gpu_update_canvas(RenderData* r, CanvasState* canvas, CanvasView* view)
{
    v2i center = view->zoom_center;
    v2l pan = view->pan_center;
    v2i new_render_center = VEC2I(pan / (i64)(1<<RENDER_CHUNK_SIZE_LOG2));
    if ( new_render_center != r->render_center ) {
        milton_log("Moving to new render center. %d, %d Clearing render data.\n", new_render_center.x, new_render_center.y);
        r->render_center = new_render_center;
        gpu_free_strokes(r, canvas);
    }
    gl::set_uniform_vec2i(r->stroke_program, "u_pan_center", 1, relative_to_render_center(r, pan).d);
    gl::set_uniform_vec2i(r->stroke_program, "u_zoom_center", 1, center.d);
    #if STROKE_DEBUG_VIZ
        gl::set_uniform_vec2i(r->stroke_debug_program, "u_pan_center", 1, relative_to_render_center(r, pan).d);
        gl::set_uniform_vec2i(r->stroke_debug_program, "u_zoom_center", 1, center.d);
    #endif
    gpu_update_scale(r, view->scale);
    float fscreen[] = { (float)view->screen_size.x, (float)view->screen_size.y };
    set_screen_size(r, fscreen);
}

void
gpu_cook_stroke(Arena* arena, RenderData* r, Stroke* stroke, CookStrokeOpt cook_option)
{
    r->stroke_z = (r->stroke_z + 1) % (MAX_DEPTH_VALUE-1);
    const i32 stroke_z = r->stroke_z + 1;

    if ( cook_option == CookStroke_NEW && stroke->render_element.vbo_stroke != 0 ) {
        // We already have our data cooked
        mlt_assert(stroke->render_element.vbo_pointa != 0);
        mlt_assert(stroke->render_element.vbo_pointb != 0);
    } else {
        auto npoints = stroke->num_points;
        if ( npoints == 1 ) {
            // Create a 2-point stroke and recurse
            Stroke duplicate = *stroke;
            duplicate.num_points = 2;
            Arena scratch_arena = arena_push(arena);
            duplicate.points = arena_alloc_array(&scratch_arena, 2, v2l);
            duplicate.pressures = arena_alloc_array(&scratch_arena, 2, f32);
            duplicate.points[0] = stroke->points[0];  // It will be set relative to the center in the recursed call.
            duplicate.points[1] = stroke->points[0];
            duplicate.pressures[0] = stroke->pressures[0];
            duplicate.pressures[1] = stroke->pressures[0];

            gpu_cook_stroke(&scratch_arena, r, &duplicate, cook_option);

            // Copy render element to stroke
            stroke->render_element = duplicate.render_element;

            arena_pop(&scratch_arena);
        }
        else if ( npoints > 1 ) {
            // 3 (triangle) *
            // 2 (two per segment) *
            // N-1 (segments per stroke)
            // Reduced to 4 by using indices
            const size_t count_attribs = 4*((size_t)npoints-1);

            // 6 (3 * 2 from count_attribs)
            // N-1 (num segments)
            const size_t count_indices = 6*((size_t)npoints-1);

            size_t count_debug = 0;
#if STROKE_DEBUG_VIZ
            count_debug = count_attribs;
#endif
            v3f* bounds;
            v3f* apoints;
            v3f* bpoints;
            v3f* debug = NULL;
            u16* indices;
            Arena scratch_arena = arena_push(arena,
                                             count_attribs*sizeof(decltype(*bounds))  // Bounds
                                             + 2*count_attribs*sizeof(decltype(*apoints)) // Attributes a,b
                                             + count_debug*sizeof(decltype(*debug))    // Visualization
                                             + count_indices*sizeof(decltype(*indices)));  // Indices

            bounds  = arena_alloc_array(&scratch_arena, count_attribs, v3f);
            apoints = arena_alloc_array(&scratch_arena, count_attribs, v3f);
            bpoints = arena_alloc_array(&scratch_arena, count_attribs, v3f);
            indices = arena_alloc_array(&scratch_arena, count_indices, u16);
#if STROKE_DEBUG_VIZ
            debug = arena_alloc_array(&scratch_arena, count_debug, v3f);
#endif

            mlt_assert(r->scale > 0);

            size_t bounds_i = 0;
            size_t apoints_i = 0;
            size_t bpoints_i = 0;
            size_t indices_i = 0;
            size_t debug_i = 0;
            for ( i64 i=0; i < npoints-1; ++i ) {
                v2i point_i = relative_to_render_center(r, stroke->points[i]);
                v2i point_j = relative_to_render_center(r, stroke->points[i+1]);

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
                bounds[bounds_i++]   = { (float)max_x, (float)min_y, (float)stroke_z };

                indices[indices_i++] = (u16)(idx + 0);
                indices[indices_i++] = (u16)(idx + 1);
                indices[indices_i++] = (u16)(idx + 2);

                indices[indices_i++] = (u16)(idx + 2);
                indices[indices_i++] = (u16)(idx + 0);
                indices[indices_i++] = (u16)(idx + 3);


                float pressure_a = stroke->pressures[i];
                float pressure_b = stroke->pressures[i+1];

                // Add attributes for each new vertex.
                for ( int repeat = 0; repeat < 4; ++repeat ) {
                    apoints[apoints_i++] = { (float)point_i.x, (float)point_i.y, pressure_a };
                    bpoints[bpoints_i++] = { (float)point_j.x, (float)point_j.y, pressure_b };
                    #if STROKE_DEBUG_VIZ
                        v3f debug_color;

                        if ( stroke->debug_flags[i] & Stroke::INTERPOLATED ) {
                            debug_color = { 1.0f, 0.0f, 0.0f };
                        }
                        else {
                            debug_color = { 0.0f, 1.0f, 0.0f };
                        }
                        debug[debug_i++] = debug_color;
                    #endif

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
            GLuint vbo_debug = 0;


            GLenum hint = GL_STATIC_DRAW;
            if ( cook_option == CookStroke_UPDATE_WORKING_STROKE ) {
                hint = GL_DYNAMIC_DRAW;
            }
            if ( stroke->render_element.vbo_stroke != 0 ) {
                vbo_stroke = stroke->render_element.vbo_stroke;
                vbo_pointa = stroke->render_element.vbo_pointa;
                vbo_pointb = stroke->render_element.vbo_pointb;
                indices_buffer = stroke->render_element.indices;
                #if STROKE_DEBUG_VIZ
                    vbo_debug = stroke->render_element.vbo_debug;
                #endif

                auto clear_array_buffer = [hint](GLint vbo, size_t size) {
                  glBindBuffer(GL_ARRAY_BUFFER, vbo);
                  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(size), NULL, hint);
                };
                clear_array_buffer(vbo_stroke, bounds_i*sizeof(decltype(*bounds)));
                clear_array_buffer(vbo_pointa, bounds_i*sizeof(decltype(*apoints)));
                clear_array_buffer(vbo_pointb, bounds_i*sizeof(decltype(*bpoints)));
                clear_array_buffer(indices_buffer, indices_i*sizeof(decltype(*indices)));
                #if STROKE_DEBUG_VIZ
                    clear_array_buffer(vbo_debug, debug_i*sizeof(decltype(*debug)));
                #endif
            }
            else {
                glGenBuffers(1, &vbo_stroke);
                glGenBuffers(1, &vbo_pointa);
                glGenBuffers(1, &vbo_pointb);
                glGenBuffers(1, &indices_buffer);
                #if STROKE_DEBUG_VIZ
                    glGenBuffers(1, &vbo_debug);
                #endif

                DEBUG_gl_mark_buffer(vbo_stroke);
                DEBUG_gl_mark_buffer(vbo_pointa);
                DEBUG_gl_mark_buffer(vbo_pointb);
                DEBUG_gl_mark_buffer(indices_buffer);
            }

            /*Send data to GPU*/ {
                auto send_buffer_data = [hint](GLint vbo, size_t size, void* data) {
                  glBindBuffer(GL_ARRAY_BUFFER, vbo);
                  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(size), data, hint);
                };

                send_buffer_data(vbo_stroke, bounds_i*sizeof(decltype(*bounds)), bounds);
                send_buffer_data(vbo_pointa, bounds_i*sizeof(decltype(*apoints)), apoints);
                send_buffer_data(vbo_pointb, bounds_i*sizeof(decltype(*bpoints)), bpoints);
                send_buffer_data(indices_buffer, indices_i*sizeof(decltype(*indices)), indices);
                #if STROKE_DEBUG_VIZ
                    send_buffer_data(vbo_debug, debug_i*sizeof(decltype(*debug)), debug);
                #endif
            }

            RenderElement re = stroke->render_element;
            re.vbo_stroke = vbo_stroke;
            re.vbo_pointa = vbo_pointa;
            re.vbo_pointb = vbo_pointb;
            re.indices = indices_buffer;
            #if STROKE_DEBUG_VIZ
                re.vbo_debug = vbo_debug;
            #endif
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
gpu_free_strokes(Stroke* strokes, i64 count, RenderData* r)
{
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

            glDeleteBuffers(1, &re->vbo_stroke);
            glDeleteBuffers(1, &re->vbo_pointa);
            glDeleteBuffers(1, &re->vbo_pointb);
            glDeleteBuffers(1, &re->indices);

            DEBUG_gl_unmark_buffer(re->vbo_stroke);
            DEBUG_gl_unmark_buffer(re->vbo_pointa);
            DEBUG_gl_unmark_buffer(re->vbo_pointb);
            DEBUG_gl_unmark_buffer(re->indices);

            *re = {};
        }
    }
}

void
gpu_free_strokes(RenderData* r, CanvasState* canvas)
{
    if ( canvas->root_layer != NULL ) {
        for ( Layer* l = canvas->root_layer;
              l != NULL;
              l = l->next ) {
            StrokeList* sl = &l->strokes;
            StrokeBucket* bucket = &sl->root;
            i64 count = sl->count;
            while ( bucket ) {
                if ( count >= STROKELIST_BUCKET_COUNT ) {
                    count -= STROKELIST_BUCKET_COUNT;
                    gpu_free_strokes(bucket->data, STROKELIST_BUCKET_COUNT, r);
                } else {
                    gpu_free_strokes(bucket->data, count, r);
                }
                bucket = bucket->next;
            }
        }
    }
}

void
gpu_clip_strokes_and_update(Arena* arena,
                            RenderData* r,
                            CanvasView* view,
                            Layer* root_layer, Stroke* working_stroke,
                            i32 x, i32 y, i32 w, i32 h, ClipFlags flags)
{
    DArray<RenderElement>* clip_array = &r->clip_array;

    RenderElement layer_element = {};
    layer_element.flags |= RenderElementFlags_LAYER;

    Rect screen_bounds;

    screen_bounds.left = x;
    screen_bounds.right = x + w;
    screen_bounds.top = y;
    screen_bounds.bottom = y+h;

    reset(clip_array);
    #if MILTON_ENABLE_PROFILING
    {
        r->clipped_count = 0;
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
            if ( l->strokes.count < bucket_i * STROKELIST_BUCKET_COUNT ) {
               // There is an allocated bucket but we have already iterated
               // through all the actual strokes.
               break;
            }
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
                        // Area might be 0 if the stroke is smaller than
                        // a pixel. We don't draw it in that case.
                        if ( !is_outside && area!=0 ) {
                            gpu_cook_stroke(arena, r, s);
                            push(clip_array, s->render_element);
                        }
                        else if ( is_outside && ( flags & ClipFlags_UPDATE_GPU_DATA ) ) {
                            // If it is far away, delete.
                            i32 distance = MLT_ABS(bounds.left - x + bounds.top - y);
                            const i32 min_number_of_screens = 4;
                            if (    bounds.top    < y - min_number_of_screens*h
                                 || bounds.bottom > y+h + min_number_of_screens*h
                                 || bounds.left   > x+w + min_number_of_screens*w
                                 || bounds.right  < x - min_number_of_screens*w ) {
                                gpu_free_strokes(s, 1, r);
                            }
                        }
                    }
                }
            }
            else if ( flags & ClipFlags_UPDATE_GPU_DATA ) {
                gpu_free_strokes(bucket->data, count, r);
            }
            #if MILTON_ENABLE_PROFILING
            {
                for ( i64 i = 0; i < count; ++i ) {
                    Stroke* s = &bucket->data[i];
                    if ( s->render_element.vbo_stroke != 0 ) {
                        r->clipped_count++;
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
                gpu_cook_stroke(arena, r, working_stroke, CookStroke_UPDATE_WORKING_STROKE);

                push(clip_array, working_stroke->render_element);
            }
        }

        auto* p = push(clip_array, layer_element);
        p->layer_alpha = l->alpha;
        p->effects = l->effects;
    }
}

static void
gpu_fill_with_texture(RenderData* r, float alpha = 1.0f)
{
    // Assumes that texture object is already bound.
    glUseProgram(r->texture_fill_program);
    gl::set_uniform_f(r->texture_fill_program, "u_alpha", alpha);
    {
        GLint t_loc = glGetAttribLocation(r->texture_fill_program, "a_position");
        if ( t_loc >= 0 ) {
            glBindBuffer(GL_ARRAY_BUFFER, r->vbo_screen_quad);
            glEnableVertexAttribArray((GLuint)t_loc);
            glVertexAttribPointer(/*attrib location*/ (GLuint)t_loc,
                                  /*size*/ 2, GL_FLOAT, /*normalize*/ GL_FALSE,
                                  /*stride*/ 0, /*ptr*/ 0);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }
}

enum BoxFilterPass
{
    BoxFilterPass_VERTICAL = 0,
    BoxFilterPass_HORIZONTAL = 1,
};
static void
box_filter_pass(RenderData* r, int kernel_size, int direction)
{
    glUseProgram(r->blur_program);
    gl::set_uniform_i(r->blur_program, "u_kernel_size", kernel_size);
    GLint t_loc = glGetAttribLocation(r->blur_program, "a_position");
    if ( t_loc >= 0 ) {
        gl::set_uniform_i(r->blur_program, "u_direction", direction);
        {
            glBindBuffer(GL_ARRAY_BUFFER, r->vbo_screen_quad);
            glEnableVertexAttribArray((GLuint)t_loc);
            glVertexAttribPointer(/*attrib location*/ (GLuint)t_loc,
                                  /*size*/ 2, GL_FLOAT, /*normalize*/ GL_FALSE,
                                  /*stride*/ 0, /*ptr*/ 0);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }
}

static void
gpu_render_canvas(RenderData* r, i32 view_x, i32 view_y,
                  i32 view_width, i32 view_height, float background_alpha=1.0f)
{
    // FLip it. GL is bottom-left.
    i32 x = view_x;
    i32 y = r->height - (view_y+view_height);
    i32 w = view_width;
    i32 h = view_height;
    glScissor(x, y, w, h);

    glClearDepth(0.0f);

    glBindFramebufferEXT(GL_FRAMEBUFFER, r->fbo);

    GLenum texture_target;
#if MULTISAMPLING_ENABLED
    if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        texture_target = GL_TEXTURE_2D_MULTISAMPLE;
    } else
#endif
    {
        texture_target = GL_TEXTURE_2D;
    }

    GLuint layer_texture = r->helper_texture;

    if ( background_alpha != 0.0f ) {
        // Not sure if this works OK with background_alpha != 1.0f
        glClearColor(r->background_color.r, r->background_color.g,
                     r->background_color.b, background_alpha);
    } else {
        glClearColor(0,0,0,0);
    }

    glBindTexture(texture_target, r->eraser_texture);

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                              r->eraser_texture, 0);

    glClear(GL_COLOR_BUFFER_BIT);

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                              r->canvas_texture, 0);

    glClear(GL_COLOR_BUFFER_BIT);

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                              layer_texture, 0);
    glClearColor(0,0,0,0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_NOTEQUAL);

    glUseProgram(r->stroke_program);

    DArray<RenderElement>* clip_array = &r->clip_array;

    for ( i64 i = 0; i < (i64)clip_array->count; i++ ) {
        RenderElement* re = &clip_array->data[i];

        if ( re->flags & RenderElementFlags_LAYER ) {

            // Layer render element.
            // The current framebuffer's color attachment is layer_texture.

            // Before we fill canvas_texture with the contents of
            // layer_texture, we apply all layer effects.

            GLuint layer_post_effects = layer_texture;
            {
                // eraser_texture will be rewritten below with the
                // contents of canvas_texture. We use it here for
                // the layer effects.
                GLuint out_texture = r->eraser_texture;
                GLuint in_texture  = layer_texture;
                glDisable(GL_BLEND);
                glDisable(GL_DEPTH_TEST);
                for ( LayerEffect* e = re->effects; e != NULL; e = e->next ) {
                    if ( e->enabled == false ) { continue; }

                    if ( (r->flags & RenderDataFlags_WITH_BLUR) && e->type == LayerEffectType_BLUR ) {
                        glBindTexture(texture_target, in_texture);
                        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                  texture_target, out_texture, 0);

                        // Three box filter iterations approximate a Gaussian blur
                        for (int blur_iter = 0; blur_iter < 3; ++blur_iter) {
                            // Box filter implementation uses the separable property.
                            // Apply horizontal pass and then vertical pass.
                            int kernel_size = e->blur.kernel_size * e->blur.original_scale / r->scale;
                            kernel_size = min(200, kernel_size);
                            box_filter_pass(r, kernel_size, BoxFilterPass_VERTICAL);
                            swap(out_texture, in_texture);
                            glBindTexture(texture_target, in_texture);
                            glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                      texture_target, out_texture, 0);


                            box_filter_pass(r, kernel_size, BoxFilterPass_HORIZONTAL);
                            swap(out_texture, in_texture);
                            glBindTexture(texture_target, in_texture);
                            glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                      texture_target, out_texture, 0);

                        }
                        swap(out_texture, in_texture);
                        glBindTexture(texture_target, in_texture);
                        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                  texture_target, out_texture, 0);
                        layer_post_effects = out_texture;
                    }
                }
                glEnable(GL_BLEND);
                glEnable(GL_DEPTH_TEST);
            }

            // Blit layer contents to canvas_texture
            {
                glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          texture_target, r->canvas_texture, 0);
                glBindTexture(texture_target, layer_post_effects);

                glDisable(GL_DEPTH_TEST);

                gpu_fill_with_texture(r, re->layer_alpha);

                glEnable(GL_DEPTH_TEST);
            }

            // Copy canvas_texture's contents to the eraser_texture.
            {
                glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          texture_target, r->eraser_texture, 0);
                glBindTexture(texture_target, r->canvas_texture);

                glDisable(GL_BLEND);
                glDisable(GL_DEPTH_TEST);

                gpu_fill_with_texture(r);

                // Clear the layer texture.
                glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          texture_target, layer_texture, 0);
                glClearColor(0,0,0,0);
                glClear(GL_COLOR_BUFFER_BIT);

                glBindTexture(texture_target, r->eraser_texture);
                glUseProgram(r->stroke_program);

                glEnable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
            }
        }
        // If this render element is not a layer, then it is a stroke.
        else {
            i64 count = re->count;

            if ( count > 0 ) {
                if ( !(r->current_color == re->color) ) {
                    gl::set_uniform_vec4(r->stroke_program, "u_brush_color", 1, re->color.d);
                    #if STROKE_DEBUG_VIZ
                        gl::set_uniform_vec4(r->stroke_debug_program, "u_brush_color", 1, re->color.d);
                    #endif
                    r->current_color = re->color;
                }
                if ( r->current_radius != re->radius ) {
                    gl::set_uniform_i(r->stroke_program, "u_radius", re->radius);
                    #if STROKE_DEBUG_VIZ
                        gl::set_uniform_i(r->stroke_debug_program, "u_radius", re->radius);
                    #endif
                    r->current_radius = re->radius;
                }

                DEBUG_gl_validate_buffer(re->vbo_stroke);
                DEBUG_gl_validate_buffer(re->vbo_pointa);
                DEBUG_gl_validate_buffer(re->vbo_pointb);
                DEBUG_gl_validate_buffer(re->indices);

                gl::vertex_attrib_v3f(r->stroke_program, "a_pointa", re->vbo_pointa);
                gl::vertex_attrib_v3f(r->stroke_program, "a_pointb", re->vbo_pointb);
                gl::vertex_attrib_v3f(r->stroke_program, "a_position", re->vbo_stroke);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, re->indices);

                // Disable blending if this element is an eraser brush stroke.
                if ( is_eraser(re->color) ) {
                    glDisable(GL_BLEND);
                    glBindTexture(texture_target, r->eraser_texture);
                    #if STROKE_DEBUG_VIZ
                        gl::set_uniform_i(r->stroke_debug_program, "u_canvas", 0);
                    #endif
                    gl::set_uniform_i(r->stroke_program, "u_canvas", 0);
                }

                glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, 0);

                #if STROKE_DEBUG_VIZ
                    glUseProgram(r->stroke_debug_program);
                    glDisable(GL_DEPTH_TEST);

                    gl::vertex_attrib_v3f(r->stroke_debug_program, "a_pointa", re->vbo_pointa);
                    gl::vertex_attrib_v3f(r->stroke_debug_program, "a_pointb", re->vbo_pointb);
                    gl::vertex_attrib_v3f(r->stroke_debug_program, "a_debug_color", re->vbo_debug);
                    gl::vertex_attrib_v3f(r->stroke_debug_program, "a_position", re->vbo_stroke);

                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, re->indices);

                    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, 0);
                    glUseProgram(r->stroke_program);
                    glEnable(GL_DEPTH_TEST);
                #endif

                if ( is_eraser(re->color) ) {
                    glEnable(GL_BLEND);
                }

            } else {
                static int n = 0;
                milton_log("Warning: Render element with count 0 [%d times]\n", ++  n);
            }
        }
    }
    glViewport(0, 0, r->width, r->height);
    glScissor(0, 0, r->width, r->height);
}

void
gpu_render(RenderData* r,  i32 view_x, i32 view_y, i32 view_width, i32 view_height)
{
    glViewport(0, 0, r->width, r->height);
    glScissor(0, 0, r->width, r->height);
    glEnable(GL_BLEND);

    print_framebuffer_status();

    gpu_render_canvas(r, view_x, view_y, view_width, view_height);

    GLenum texture_target;
#if MULTISAMPLING_ENABLED
    if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        texture_target = GL_TEXTURE_2D_MULTISAMPLE;
    } else
#endif
    {
        texture_target = GL_TEXTURE_2D;
    }

    // Use helper_texture as a place to do AA.

    // Blit the canvas to helper_texture

    glDisable(GL_DEPTH_TEST);

#if MULTISAMPLING_ENABLED
    if ( !gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                                  r->canvas_texture, 0);
        glBindTexture(texture_target, r->helper_texture);
        glCopyTexImage2D(texture_target, 0, GL_RGBA8, 0,0, r->width, r->height, 0);

        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                                  r->helper_texture, 0);
        glBindTexture(texture_target, r->canvas_texture);
    } else
#endif
    {
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                                  r->helper_texture, 0);
        glBindTexture(texture_target, r->canvas_texture);

        gpu_fill_with_texture(r);
    }

    // Render GUI on top of helper_texture

    // Render color picker
    // TODO: Only render if view rect intersects picker rect
    if ( r->flags & RenderDataFlags_GUI_VISIBLE ) {
        // Render picker
        glUseProgram(r->picker_program);
        GLint loc = glGetAttribLocation(r->picker_program, "a_position");

        if ( loc >= 0 ) {
            DEBUG_gl_validate_buffer(r->vbo_picker);
            glBindBuffer(GL_ARRAY_BUFFER, r->vbo_picker);
            glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                  /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                  /*stride*/0, /*ptr*/0);
            glEnableVertexAttribArray((GLuint)loc);
            GLint loc_norm = glGetAttribLocation(r->picker_program, "a_norm");

            if ( loc_norm >= 0 ) {
                DEBUG_gl_validate_buffer(r->vbo_picker_norm);
                glBindBuffer(GL_ARRAY_BUFFER, r->vbo_picker_norm);
                glVertexAttribPointer(/*attrib location*/(GLuint)loc_norm,
                                      /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                      /*stride*/0, /*ptr*/0);
                glEnableVertexAttribArray((GLuint)loc_norm);

            }
            glDrawArrays(GL_TRIANGLE_FAN,0,4);
        }
    }

    // Do post-processing on painting and on GUI elements. Draw to backbuffer

#if MULTISAMPLING_ENABLED
    if ( !gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        glBindFramebufferEXT(GL_FRAMEBUFFER, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, r->helper_texture);

        gl::set_uniform_i(r->postproc_program, "u_canvas", 0);

        glUseProgram(r->postproc_program);

        GLint loc = glGetAttribLocation(r->postproc_program, "a_position");
        if ( loc >= 0 ) {
            DEBUG_gl_validate_buffer(r->vbo_screen_quad);
            glBindBuffer(GL_ARRAY_BUFFER, r->vbo_screen_quad);
            glVertexAttribPointer((GLuint)loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray((GLuint)loc);
            glVertexAttribPointer(/*attrib location*/ (GLuint)loc,
                                  /*size*/ 2, GL_FLOAT, /*normalize*/ GL_FALSE,
                                  /*stride*/ 0, /*ptr*/ 0);

            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }
    else
#endif
    {  // Resolve
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, r->fbo);
        glBlitFramebufferEXT(0, 0, r->width, r->height,
                             0, 0, r->width, r->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    // Render outlines after doing AA.

    // Brush outline
    {
        glUseProgram(r->outline_program);
        GLint loc = glGetAttribLocation(r->outline_program, "a_position");
        if ( loc >= 0 ) {
            DEBUG_gl_validate_buffer(r->vbo_outline);
            glBindBuffer(GL_ARRAY_BUFFER, r->vbo_outline);

            glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                  /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                  /*stride*/0, /*ptr*/0);
            glEnableVertexAttribArray((GLuint)loc);
            GLint loc_s = glGetAttribLocation(r->outline_program, "a_sizes");
            if ( loc_s >= 0 ) {
                DEBUG_gl_validate_buffer(r->vbo_outline_sizes);
                glBindBuffer(GL_ARRAY_BUFFER, r->vbo_outline_sizes);
                glVertexAttribPointer(/*attrib location*/(GLuint)loc_s,
                                      /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                      /*stride*/0, /*ptr*/0);
                glEnableVertexAttribArray((GLuint)loc_s);
            }
        }
        glDrawArrays(GL_TRIANGLE_FAN, 0,4);
    }
    glDisable(GL_BLEND);

    // Exporter rect
    if ( r->flags & RenderDataFlags_EXPORTING ) {
        // Update data if rect is not degenerate.
        // Draw outline.
        glUseProgram(r->exporter_program);
        GLint loc = glGetAttribLocation(r->exporter_program, "a_position");
        if ( loc>=0 && r->vbo_exporter[0] > 0 ) {
            for ( int vbo_i = 0; vbo_i < 4; ++vbo_i ) {
                DEBUG_gl_validate_buffer(r->vbo_exporter[vbo_i]);
                glBindBuffer(GL_ARRAY_BUFFER, r->vbo_exporter[vbo_i]);
                glVertexAttribPointer((GLuint)loc, 2, GL_FLOAT, GL_FALSE, 0,0);
                glEnableVertexAttribArray((GLuint)loc);

                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
        }
    }

    glUseProgram(0);
}

void
gpu_render_to_buffer(Milton* milton, u8* buffer, i32 scale, i32 x, i32 y, i32 w, i32 h, f32 background_alpha)
{
    CanvasView saved_view = *milton->view;
    RenderData* r = milton->render_data;
    CanvasView* view = milton->view;

    i32 saved_width = r->width;
    i32 saved_height = r->height;
    GLuint saved_fbo = r->fbo;

    i32 buf_w = w * scale;
    i32 buf_h = h * scale;

    v2i center = milton->view->screen_size / 2;
    v2i pan_delta = v2i{x + (w / 2), y + (h / 2)} - center;

    milton_set_zoom_at_point(milton, center);

    milton->view->pan_center =
        milton->view->pan_center + VEC2L(pan_delta)*milton->view->scale;

    milton->view->screen_size = v2i{buf_w, buf_h};
    r->width = buf_w;
    r->height = buf_h;

    milton->view->zoom_center = milton->view->screen_size / 2;
    if ( scale > 1 ) {
        milton->view->scale = (i32)ceill(((f32)milton->view->scale / (f32)scale));
    }

    gpu_resize(r, view);
    gpu_update_canvas(r, milton->canvas, view);

    // TODO: Check for out-of-memory errors.

    mlt_assert(buf_w == r->width);
    mlt_assert(buf_h == r->height);

    glViewport(0, 0, buf_w, buf_h);
    glScissor(0, 0, buf_w, buf_h);
    gpu_clip_strokes_and_update(&milton->root_arena, r, milton->view, milton->canvas->root_layer,
                                &milton->working_stroke, 0, 0, buf_w, buf_h);

    r->flags |= RenderDataFlags_WITH_BLUR;
    gpu_render_canvas(r, 0, 0, buf_w, buf_h, background_alpha);


    // Post processing
#if MULTISAMPLING_ENABLED
    if ( !gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        glUseProgram(r->postproc_program);
        glBindTexture(GL_TEXTURE_2D, r->canvas_texture);

        GLint loc = glGetAttribLocation(r->postproc_program, "a_position");
        if ( loc >= 0 ) {
            DEBUG_gl_validate_buffer(r->vbo_screen_quad);
            glBindBuffer(GL_ARRAY_BUFFER, r->vbo_screen_quad);
            glVertexAttribPointer((GLuint)loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray((GLuint)loc);

            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    } else
#endif
    {
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, r->fbo);
        glBlitFramebufferEXT(0, 0, buf_w, buf_h,
                             0, 0, buf_w, buf_h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
    }

    glEnable(GL_DEPTH_TEST);

    // Read onto buffer
    glReadPixels(0,0,
                 buf_w, buf_h,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 (GLvoid*)buffer);

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

    r->fbo = saved_fbo;
    *milton->view = saved_view;
    r->width = saved_width;
    r->height = saved_height;

    glBindFramebufferEXT(GL_FRAMEBUFFER, r->fbo);

    gpu_resize(r, view);
    gpu_update_canvas(r, milton->canvas, view);

    // Re-render
    gpu_clip_strokes_and_update(&milton->root_arena,
                                r, milton->view, milton->canvas->root_layer,
                                &milton->working_stroke, 0, 0, r->width,
                                r->height);
    gpu_render(r, 0, 0, r->width, r->height);
}

void
gpu_release_data(RenderData* r)
{
    release(&r->clip_array);
}


