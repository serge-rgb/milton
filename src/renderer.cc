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


enum ImmediateFlag
{
    ImmediateFlag_RECT = (1<<0),
};

// Draw data for single stroke
enum RenderElementFlags
{
    RenderElementFlags_NONE = 0,
    RenderElementFlags_LAYER                = 1<<0,
    RenderElementFlags_PRESSURE_TO_OPACITY  = 1<<1,
    RenderElementFlags_DISTANCE_TO_OPACITY  = 1<<2,
    RenderElementFlags_ERASER               = 1<<3,
};

struct RenderElement
{
    GLuint  vbo_stroke;
    GLuint  vbo_pointa;
    GLuint  vbo_pointb;
    GLuint  indices;
#if STROKE_DEBUG_VIZ
    GLuint vbo_debug;
#endif

    i64     count;

    union {
        struct {  // For when element is a stroke.
            v4f     color;
            i32     radius;
            f32     min_opacity;
            f32     hardness;
        };
        struct {  // For when element is layer.
            f32          layer_alpha;
            LayerEffect* effects;
        };
    };

    int     flags;  // RenderElementFlags enum;
};

struct RenderBackend
{
    f32 viewport_limits[2];  // OpenGL limits to the framebuffer size.

    v2i render_center;

    // OpenGL programs.

    GLuint stroke_program;
    GLuint stroke_eraser_program;

    GLuint stroke_clear_program;
    GLuint stroke_info_program;

    // TODO: If we add more variations we probably will want to automate this...
    GLuint stroke_fill_program_pressure;
    GLuint stroke_fill_program_distance;
    GLuint stroke_fill_program_pressure_distance;

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
    GLuint vbo_exporter;
    GLuint exporter_indices;
    int exporter_indices_count;

    u32 imm_flags;  // ImmediateFlag

    // Objects used in rendering.
    GLuint canvas_texture;
    GLuint eraser_texture;
    GLuint helper_texture;  // Used for various effects..
    GLuint stencil_texture;
    GLuint stroke_info_texture;

    GLuint fbo;

    i32 flags;  // RenderBackendFlags enum

    DArray<RenderElement> clip_array;

    // Screen size.
    i32 width;
    i32 height;

    v3f background_color;
    i32 scale;  // zoom

    // See MAX_DEPTH_VALUE
    i32 stroke_z;

    // TODO: Re-enable these?
    // Cached values for stroke rendering uniforms.
    // v4f current_color;
    // float current_radius;

#if MILTON_ENABLE_PROFILING
    u64 clipped_count;
#endif
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

static RenderElement*
get_render_element(RenderHandle handle)
{
    RenderElement* e = reinterpret_cast<RenderElement*>(handle);
    return e;
}

RenderBackend*
gpu_allocate_render_backend(Arena* arena)
{
    RenderBackend* p = arena_alloc_elem(arena, RenderBackend);
    return p;
}

// Send Color Picker data to OpenGL.
void
gpu_update_picker(RenderBackend* r, ColorPicker* picker)
{
    gl::use_program(r->picker_program);
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
gpu_update_brush_outline(RenderBackend* r, i32 cx, i32 cy, i32 radius,
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

GL_DEBUG_CALLBACK(milton_gl_debug_callback)
{
    switch ( type ) {
        case GL_DEBUG_TYPE_ERROR:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        case GL_DEBUG_SOURCE_APPLICATION: {
            milton_log("[OpenGl Debug message (severity: %d)]: %s\n", severity, message);
        } break;
    }
}


b32
gpu_init(RenderBackend* r, CanvasView* view, ColorPicker* picker)
{
    #if MILTON_DEBUG
        glEnable(GL_DEBUG_OUTPUT);
        if (glDebugMessageCallback) {  glDebugMessageCallback(milton_gl_debug_callback, NULL); }
    #endif

    r->stroke_z = MAX_DEPTH_VALUE - 20;

    if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        glEnable(GL_MULTISAMPLE);
        // TODO: remove sample shading
        if ( gl::check_flags(GLHelperFlags_SAMPLE_SHADING) ) {
            glEnable(GL_SAMPLE_SHADING_ARB);
            // glMinSampleShadingARB(1.0f);
        }
    }

    {
        GLfloat viewport_dims[2] = {};
        glGetFloatv(GL_MAX_VIEWPORT_DIMS, viewport_dims);
        milton_log("Maximum viewport dimensions, %fx%f\n", viewport_dims[0], viewport_dims[1]);
        r->viewport_limits[0] = viewport_dims[0];
        r->viewport_limits[1] = viewport_dims[1];
    }

    // r->current_color = {-1,-1,-1,-1};
    // r->current_radius = -1;

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

    GLuint stroke_vs = gl::compile_shader(g_stroke_raster_v, GL_VERTEX_SHADER);

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

        objs[0] = stroke_vs;
        objs[1] = gl::compile_shader(g_stroke_raster_f, GL_FRAGMENT_SHADER, config_string);

        r->stroke_program = glCreateProgram();

        gl::link_program(r->stroke_program, objs, array_count(objs));
    }
    // Stroke eraser
    {
        GLuint objs[2];
        objs[0] = stroke_vs;
        objs[1] = gl::compile_shader(g_stroke_eraser_f, GL_FRAGMENT_SHADER);

        r->stroke_eraser_program = glCreateProgram();
        gl::link_program(r->stroke_eraser_program, objs, array_count(objs));

        gl::set_uniform_i(r->stroke_eraser_program, "u_canvas", 0);
    }
    // Stroke info program
    {
        GLuint objs[2];
        objs[0] = stroke_vs;
        objs[1] = gl::compile_shader(g_stroke_info_f, GL_FRAGMENT_SHADER);

        r->stroke_info_program = glCreateProgram();
        gl::link_program(r->stroke_info_program, objs, array_count(objs));

    }
    // Stroke fill program
    {
        GLuint objs[2];
        objs[0] = stroke_vs;

        r->stroke_fill_program_distance = glCreateProgram();
        r->stroke_fill_program_pressure = glCreateProgram();
        r->stroke_fill_program_pressure_distance = glCreateProgram();

        objs[1] = gl::compile_shader(g_stroke_fill_f, GL_FRAGMENT_SHADER);
        gl::link_program(r->stroke_fill_program_pressure, objs, array_count(objs));

        objs[1] = gl::compile_shader(g_stroke_fill_f, GL_FRAGMENT_SHADER, "", "#define DISTANCE_TO_OPACITY 1\n#define PRESSURE_TO_OPACITY 0\n");
        gl::link_program(r->stroke_fill_program_distance, objs, array_count(objs));

        objs[1] = gl::compile_shader(g_stroke_fill_f, GL_FRAGMENT_SHADER, "", "#define DISTANCE_TO_OPACITY 1\n");
        gl::link_program(r->stroke_fill_program_pressure_distance, objs, array_count(objs));

        GLuint ps[] = {
            r->stroke_fill_program_pressure,
            r->stroke_fill_program_pressure_distance,
            r->stroke_fill_program_distance,
        };
        for (auto p : ps) {
            gl::set_uniform_i(p, "u_info", 0);
        }
    }
    // Stroke clear program
    {
        GLuint objs[2];
        objs[0] = stroke_vs;
        objs[1] = gl::compile_shader(g_stroke_clear_f, GL_FRAGMENT_SHADER);

        r->stroke_clear_program = glCreateProgram();
        gl::link_program(r->stroke_clear_program, objs, array_count(objs));
    }
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
        if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            r->canvas_texture = gl::new_color_texture_multisample(view->screen_size.w, view->screen_size.h);
        } else {
            r->canvas_texture = gl::new_color_texture(view->screen_size.w, view->screen_size.h);
        }

        if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            r->eraser_texture = gl::new_color_texture_multisample(view->screen_size.w, view->screen_size.h);
        } else {
            r->eraser_texture = gl::new_color_texture(view->screen_size.w, view->screen_size.h);
        }

        glGenTextures(1, &r->helper_texture);

        if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            r->helper_texture = gl::new_color_texture_multisample(view->screen_size.w, view->screen_size.h);
        } else {
            r->helper_texture = gl::new_color_texture(view->screen_size.w, view->screen_size.h);
        }

        // Stroke info buffer
        {
            r->stroke_info_texture = gl::new_color_texture(view->screen_size.w, view->screen_size.h);
            print_framebuffer_status();
        }


        glGenTextures(1, &r->stencil_texture);

        if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            r->stencil_texture = gl::new_depth_stencil_texture_multisample(view->screen_size.w, view->screen_size.h);
        }
        else {
            r->stencil_texture = gl::new_depth_stencil_texture(view->screen_size.w, view->screen_size.h);
        }

        // Create framebuffer object.
        GLenum texture_target;
        if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
            texture_target = GL_TEXTURE_2D_MULTISAMPLE;
        }
        else{
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
gpu_resize(RenderBackend* r, CanvasView* view)
{
    r->width = view->screen_size.w;
    r->height = view->screen_size.h;

    if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        gl::resize_color_texture_multisample(r->eraser_texture, r->width, r->height);
        gl::resize_color_texture_multisample(r->canvas_texture, r->width, r->height);
        gl::resize_color_texture_multisample(r->helper_texture, r->width, r->height);
        gl::resize_depth_stencil_texture_multisample(r->stencil_texture, r->width, r->height);
    }
    else {
        gl::resize_color_texture(r->eraser_texture, r->width, r->height);
        gl::resize_color_texture(r->canvas_texture, r->width, r->height);
        gl::resize_color_texture(r->helper_texture, r->width, r->height);
        gl::resize_color_texture(r->stroke_info_texture, r->width, r->height);
        gl::resize_depth_stencil_texture(r->stencil_texture, r->width, r->height);
    }
}

void
gpu_reset_render_flags(RenderBackend* r, int flags)
{
    r->flags = flags;
}

void
gpu_update_scale(RenderBackend* r, i32 scale)
{
    r->scale = scale;
    GLuint ps[] = {
        r->stroke_program,
        r->stroke_eraser_program,
        r->stroke_info_program,
        r->stroke_fill_program_pressure,
        r->stroke_fill_program_pressure_distance,
        r->stroke_fill_program_distance,
        r->stroke_clear_program,
    };
    for (sz i = 0; i < array_count(ps); ++i) {
        gl::set_uniform_i(ps[i], "u_scale", scale);
    }
}

void
gpu_update_export_rect(RenderBackend* r, Exporter* exporter)
{
    if ( r->vbo_exporter == 0 ) {
        glGenBuffers(1, &r->vbo_exporter);
        mlt_assert(r->exporter_indices == 0);
        glGenBuffers(1, &r->exporter_indices);
    }

    i32 x = min(exporter->pivot.x, exporter->needle.x);
    i32 y = min(exporter->pivot.y, exporter->needle.y);
    i32 w = MLT_ABS(exporter->pivot.x - exporter->needle.x);
    i32 h = MLT_ABS(exporter->pivot.y - exporter->needle.y);

    float left = 2*((float)    x/(r->width))-1;
    float right = 2*((GLfloat)(x+w)/(r->width))-1;
    float top = -(2*((GLfloat)y    /(r->height))-1);
    float bottom = -(2*((GLfloat)(y+h)/(r->height))-1);

    // Normalize to [-1,1]^2
    v2f normalized_rect[] = {
        { 2*((GLfloat)    x/(r->width))-1, -(2*((GLfloat)y    /(r->height))-1) },
        { 2*((GLfloat)    x/(r->width))-1, -(2*((GLfloat)(y+h)/(r->height))-1) },
        { 2*((GLfloat)(x+w)/(r->width))-1, -(2*((GLfloat)(y+h)/(r->height))-1) },
        { 2*((GLfloat)(x+w)/(r->width))-1, -(2*((GLfloat)y    /(r->height))-1) },
    };

    float px = 2.0f;
    float line_width = px / r->height;

    float toparr[] = {
        // Top quad
        left, top - line_width/2,
        left, top + line_width/2,
        right, top + line_width/2,
        right, top - line_width/2,

        // Bottom quad
        left, bottom-line_width/2,
        left, bottom+line_width/2,
        right, bottom+line_width/2,
        right, bottom-line_width/2,

        // Left
        left-line_width/2, top,
        left-line_width/2, bottom,
        left+line_width/2, bottom,
        left+line_width/2, top,

        // Right
        right-line_width/2, top,
        right-line_width/2, bottom,
        right+line_width/2, bottom,
        right+line_width/2, top,
    };
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_exporter);
    DEBUG_gl_mark_buffer(r->vbo_exporter);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(toparr)*sizeof(*toparr), toparr, GL_DYNAMIC_DRAW);

    u16 indices[] = {
        // top
        0,1,2,
        2,3,0,

        // bottom
        4,5,6,
        6,7,4,

        // left
        8,9,10,
        10,11,8,

        // right
        12,13,14,
        14,15,12,
    };
    glBindBuffer(GL_ARRAY_BUFFER, r->exporter_indices);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(indices)*sizeof(*indices), indices, GL_STATIC_DRAW);

    r->exporter_indices_count = array_count(indices);
}

void
gpu_update_background(RenderBackend* r, v3f background_color)
{
    r->background_color = background_color;
}

void
gpu_get_viewport_limits(RenderBackend* r, float* out_viewport_limits)
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
            RenderElement* re = get_render_element(s->render_handle);
            if ( re && re->vbo_stroke != 0 ) {
                ++count;
            }
        }
    }
    #endif
    return count;
}

static void
set_screen_size(RenderBackend* r, float* fscreen)
{
    GLuint programs[] = {
        r->stroke_program,
        r->stroke_eraser_program,
        r->stroke_info_program,
        r->stroke_fill_program_pressure,
        r->stroke_fill_program_pressure_distance,
        r->stroke_fill_program_distance,
        r->stroke_clear_program,
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
relative_to_render_center(RenderBackend* r, v2l point)
{
    v2i result = VEC2I(point - VEC2L(r->render_center*(1<<RENDER_CHUNK_SIZE_LOG2)));
    return result;
}

void
gpu_update_canvas(RenderBackend* r, CanvasState* canvas, CanvasView* view)
{
    v2i center = view->zoom_center;
    v2l pan = view->pan_center;

    v2i new_render_center = VEC2I(pan / (i64)(1<<RENDER_CHUNK_SIZE_LOG2));
    if ( new_render_center != r->render_center ) {
        milton_log("Moving to new render center. %d, %d Clearing render data.\n", new_render_center.x, new_render_center.y);
        r->render_center = new_render_center;
        gpu_free_strokes(r, canvas);
    }

    GLuint ps[] = {
        r->stroke_program,
        r->stroke_eraser_program,
        r->stroke_info_program,
        r->stroke_fill_program_pressure,
        r->stroke_fill_program_pressure_distance,
        r->stroke_fill_program_distance,
        r->stroke_clear_program,
    };

    f32 cos_angle = cosf(view->angle);
    f32 sin_angle = sinf(view->angle);

    // GLSL is column-major
    f32 matrix[] = { cos_angle, sin_angle, -sin_angle, cos_angle };

    f32 matrix_inverse[] = { cos_angle, -sin_angle, sin_angle, cos_angle };

    for (sz i = 0; i < array_count(ps); ++i) {
        gl::set_uniform_mat2(ps[i], "u_rotation", matrix);
        gl::set_uniform_mat2(ps[i], "u_rotation_inverse", matrix_inverse);
        gl::set_uniform_vec2i(ps[i], "u_pan_center", 1, relative_to_render_center(r, pan).d);
        gl::set_uniform_vec2i(ps[i], "u_zoom_center", 1, center.d);
    }

    gpu_update_scale(r, view->scale);
    float fscreen[] = { (float)view->screen_size.x, (float)view->screen_size.y };
    set_screen_size(r, fscreen);
}

void
gpu_cook_stroke(Arena* arena, RenderBackend* r, Stroke* stroke, CookStrokeOpt cook_option)
{

    RenderElement** p_render_element = reinterpret_cast<RenderElement**>(&stroke->render_handle);
    RenderElement* render_element = *p_render_element;
    if (render_element == NULL) {
        render_element = arena_alloc_elem(arena, RenderElement);
        *p_render_element = render_element;
    }

    r->stroke_z = (r->stroke_z + 1) % (MAX_DEPTH_VALUE-1);
    const i32 stroke_z = r->stroke_z + 1;

    if ( cook_option == CookStroke_NEW && render_element->vbo_stroke != 0 ) {
        // We already have our data cooked
        mlt_assert(render_element->vbo_pointa != 0);
        mlt_assert(render_element->vbo_pointb != 0);
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
            stroke->render_handle = duplicate.render_handle;

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
            v3f* bounds;
            v3f* apoints;
            v3f* bpoints;
            v3f* debug = NULL;
            u16* indices;
            Arena scratch_arena = arena_push(arena,
                                             count_attribs*sizeof(decltype(*bounds))  // Bounds
                                             + 2*count_attribs*sizeof(decltype(*apoints)) // Attributes a,b
                                             + count_debug*sizeof(decltype(*debug))    // Visualization
                                             + count_indices*sizeof(decltype(*indices)));  // Interpolation points

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

                u16 idx = (u16)bounds_i;
                if ( point_i == point_j ) {
                    i32 min_x = min(point_i.x - radius_i, point_j.x - radius_j);
                    i32 min_y = min(point_i.y - radius_i, point_j.y - radius_j);
                    i32 max_x = max(point_i.x + radius_i, point_j.x + radius_j);
                    i32 max_y = max(point_i.y + radius_i, point_j.y + radius_j);

                    // Bounding geometry and attributes

                    mlt_assert (bounds_i < ((1<<16)-4));

                    bounds[bounds_i++] = { (float)min_x, (float)min_y, (float)stroke_z };
                    bounds[bounds_i++] = { (float)min_x, (float)max_y, (float)stroke_z };
                    bounds[bounds_i++] = { (float)max_x, (float)max_y, (float)stroke_z };
                    bounds[bounds_i++] = { (float)max_x, (float)min_y, (float)stroke_z };
                } else {
                    // Points are different. Do a coordinate change for a tighter box.
                    v2f d = normalized(v2i_to_v2f(point_j - point_i));
                    auto basis_change = [&d](v2f v) {
                        v2f res = {
                            v.x * d.x + v.y * d.y,
                            v.x * d.y - v.y * d.x,
                        };

                        return res;
                    };
                    v2f a = basis_change(v2i_to_v2f(point_i));
                    v2f b = basis_change(v2i_to_v2f(point_j));

                    f32 rad = max(radius_i, radius_j);

                    f32 min_x = min(a.x, b.x) - rad;
                    f32 min_y = min(a.y, b.y) - rad;
                    f32 max_x = max(a.x, b.x) + rad;
                    f32 max_y = max(a.y, b.y) + rad;

                    v2f A = basis_change(v2f{ min_x, min_y });
                    v2f B = basis_change(v2f{ min_x, max_y });
                    v2f C = basis_change(v2f{ max_x, max_y });
                    v2f D = basis_change(v2f{ max_x, min_y });

                    mlt_assert (bounds_i < ((1<<16)-4));

                    bounds[bounds_i++] = { A.x, A.y, (float)stroke_z };
                    bounds[bounds_i++] = { B.x, B.y, (float)stroke_z };
                    bounds[bounds_i++] = { C.x, C.y, (float)stroke_z };
                    bounds[bounds_i++] = { D.x, D.y, (float)stroke_z };
                }

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
            if ( render_element->vbo_stroke != 0 ) {
                vbo_stroke = render_element->vbo_stroke;
                vbo_pointa = render_element->vbo_pointa;
                vbo_pointb = render_element->vbo_pointb;
                indices_buffer = render_element->indices;
                #if STROKE_DEBUG_VIZ
                    vbo_debug = render_element->vbo_debug;
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

            RenderElement* re = get_render_element(stroke->render_handle);
            re->vbo_stroke = vbo_stroke;
            re->vbo_pointa = vbo_pointa;
            re->vbo_pointb = vbo_pointb;
            re->indices = indices_buffer;
            #if STROKE_DEBUG_VIZ
                re->vbo_debug = vbo_debug;
            #endif
            re->count = (i64)(indices_i);
            re->color = { stroke->brush.color.r, stroke->brush.color.g, stroke->brush.color.b, stroke->brush.color.a };
            re->radius = stroke->brush.radius;
            re->min_opacity = stroke->brush.pressure_opacity_min;
            re->hardness = stroke->brush.hardness;

            re->flags = 0;
            if (stroke->flags & StrokeFlag_ERASER) {
                re->flags |= RenderElementFlags_ERASER;
            }
            if (stroke->flags & StrokeFlag_PRESSURE_TO_OPACITY) {
                re->flags |= RenderElementFlags_PRESSURE_TO_OPACITY;
            }
            if (stroke->flags & StrokeFlag_DISTANCE_TO_OPACITY) {
                re->flags |= RenderElementFlags_DISTANCE_TO_OPACITY;
            }

            mlt_assert(re->count > 1);

            arena_pop(&scratch_arena);
        }
    }
}

void
gpu_free_strokes(Stroke* strokes, i64 count, RenderBackend* r)
{
    for ( i64 i = 0; i < count; ++i ) {
        Stroke* s = &strokes[i];
        RenderElement* re = get_render_element(s->render_handle);
        if ( re && re->vbo_stroke != 0 ) {
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
gpu_free_strokes(RenderBackend* r, CanvasState* canvas)
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
                            RenderBackend* r,
                            CanvasView* view,
                            i64 scale,
                            Layer* root_layer, Stroke* working_stroke,
                            i32 x, i32 y, i32 w, i32 h, ClipFlags flags)
{
    DArray<RenderElement>* clip_array = &r->clip_array;

    RenderElement layer_element = {};
    layer_element.flags |= RenderElementFlags_LAYER;

    Rect screen_bounds = raster_to_canvas_bounding_rect(view, x, y, w, h, scale);

    reset(clip_array);

    if (screen_bounds.left != screen_bounds.right &&
        screen_bounds.top != screen_bounds.bottom) {
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

                b32 bucket_outside =   screen_bounds.left   > bbox.right
                                    || screen_bounds.top    > bbox.bottom
                                    || screen_bounds.right  < bbox.left
                                    || screen_bounds.bottom < bbox.top;

                if ( !bucket_outside ) {
                    for ( i64 i = 0; i < count; ++i ) {
                        Stroke* s = &bucket->data[i];

                        if ( s != NULL ) {
                            Rect bounds = s->bounding_rect;

                            b32 stroke_outside =   screen_bounds.left   > bounds.right
                                                || screen_bounds.top    > bounds.bottom
                                                || screen_bounds.right  < bounds.left
                                                || screen_bounds.bottom < bounds.top;

                            i32 area = (bounds.right-bounds.left) * (bounds.bottom-bounds.top);
                            // Area might be 0 if the stroke is smaller than
                            // a pixel. We don't draw it in that case.
                            if ( !stroke_outside && area!=0 ) {
                                gpu_cook_stroke(arena, r, s);
                                push(clip_array, *get_render_element(s->render_handle));
                            }
                            else if ( stroke_outside && ( flags & ClipFlags_UPDATE_GPU_DATA ) ) {
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
                        RenderElement* re = get_render_element(s->render_handle);
                        if ( re && re->vbo_stroke != 0 ) {
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

                    push(clip_array, *get_render_element(working_stroke->render_handle));
                }
            }

            auto* p = push(clip_array, layer_element);
            p->layer_alpha = l->alpha;
            p->effects = l->effects;
        }
    }
}

static void
gpu_fill_with_texture(RenderBackend* r, float alpha = 1.0f)
{
    // Assumes that texture object is already bound.
    gl::use_program(r->texture_fill_program);
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
box_filter_pass(RenderBackend* r, int kernel_size, int direction)
{
    gl::use_program(r->blur_program);
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
gpu_render_canvas(RenderBackend* r, i32 view_x, i32 view_y,
                  i32 view_width, i32 view_height, float background_alpha=1.0f)
{
    PUSH_GRAPHICS_GROUP("render_canvas");

    // FLip it. GL is bottom-left.
    i32 x = view_x;
    i32 y = r->height - (view_y+view_height);
    i32 w = view_width;
    i32 h = view_height;
    glScissor(x, y, w, h);

    glClearDepth(0.0f);

    glBindFramebufferEXT(GL_FRAMEBUFFER, r->fbo);

    GLenum texture_target;
    if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        texture_target = GL_TEXTURE_2D_MULTISAMPLE;
    } else {
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

    DArray<RenderElement>* clip_array = &r->clip_array;

    PUSH_GRAPHICS_GROUP("render elements");
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

                    if ( e->type == LayerEffectType_BLUR ) {
                        glBindTexture(texture_target, in_texture);
                        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                  texture_target, out_texture, 0);

                        // Three box filter iterations approximate a Gaussian blur
                        for (int blur_iter = 0; blur_iter < 3; ++blur_iter) {
                            // Box filter implementation uses the separable property.
                            // Apply horizontal pass and then vertical pass.
                            int kernel_size = e->blur.kernel_size * e->blur.original_scale / r->scale;
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
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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

                glEnable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
            }
        }
        // If this render element is not a layer, then it is a stroke.
        else {
            GLuint program_for_stroke = r->stroke_program;

            auto stroke_pass = [r, texture_target](RenderElement* re, GLuint program_for_stroke) {
                i64 count = re->count;
                gl::use_program(program_for_stroke);
                gl::set_uniform_vec4(program_for_stroke, "u_brush_color", 1, re->color.d);
                gl::set_uniform_i(program_for_stroke, "u_radius", re->radius);

                DEBUG_gl_validate_buffer(re->vbo_stroke);
                DEBUG_gl_validate_buffer(re->vbo_pointa);
                DEBUG_gl_validate_buffer(re->vbo_pointb);
                DEBUG_gl_validate_buffer(re->indices);

                gl::vertex_attrib_v3f(program_for_stroke, "a_pointa", re->vbo_pointa);
                gl::vertex_attrib_v3f(program_for_stroke, "a_pointb", re->vbo_pointb);
                gl::vertex_attrib_v3f(program_for_stroke, "a_position", re->vbo_stroke);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, re->indices);

                glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, 0);
            };

            if ( re->count > 0 ) {
                if (re->flags & RenderElementFlags_ERASER) {
                    glBindTexture(texture_target, r->eraser_texture);
                    stroke_pass(re, r->stroke_eraser_program);
                }
                else if ( (re->flags & (RenderElementFlags_PRESSURE_TO_OPACITY | RenderElementFlags_DISTANCE_TO_OPACITY)) ) {
                    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              texture_target, r->stroke_info_texture, 0);


                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_BLEND);
                    stroke_pass(re, r->stroke_clear_program);

                    glEnable(GL_BLEND);
                    glBlendEquationSeparate(GL_MIN, GL_MAX);

                    stroke_pass(re, r->stroke_info_program);

                    glBlendEquation(GL_FUNC_ADD);

                    glEnable(GL_DEPTH_TEST);
                    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              texture_target, layer_texture, 0);
                    glBindTexture(texture_target, r->stroke_info_texture);

                    if ( (re->flags & RenderElementFlags_PRESSURE_TO_OPACITY) &&
                        !(re->flags & RenderElementFlags_DISTANCE_TO_OPACITY)) {
                        gl::set_uniform_f(r->stroke_fill_program_pressure, "u_opacity_min", re->min_opacity);
                        stroke_pass(re, r->stroke_fill_program_pressure);
                    }
                    else if ( !(re->flags & RenderElementFlags_PRESSURE_TO_OPACITY) &&
                               (re->flags & RenderElementFlags_DISTANCE_TO_OPACITY)) {
                        gl::set_uniform_f(r->stroke_fill_program_distance, "u_hardness", re->hardness);
                        stroke_pass(re, r->stroke_fill_program_distance);
                    }
                    else if ( (re->flags & RenderElementFlags_PRESSURE_TO_OPACITY) &&
                              (re->flags & RenderElementFlags_DISTANCE_TO_OPACITY)) {
                        gl::set_uniform_f(r->stroke_fill_program_pressure_distance, "u_opacity_min", re->min_opacity);
                        gl::set_uniform_f(r->stroke_fill_program_pressure_distance, "u_hardness", re->hardness);
                        stroke_pass(re, r->stroke_fill_program_pressure_distance);
                    }
                    else {
                        INVALID_CODE_PATH;
                    }
                }
                else {
                    // Fast path
                    stroke_pass(re, r->stroke_program);
                }
            } else {
                static int n = 0;
                milton_log("Warning: Render element with count 0 [%d times]\n", ++n);
            }
        }
    }
    POP_GRAPHICS_GROUP();  // render elements
    glViewport(0, 0, r->width, r->height);
    glScissor(0, 0, r->width, r->height);

    POP_GRAPHICS_GROUP();  // render_canvas
}

void
gpu_render(RenderBackend* r,  i32 view_x, i32 view_y, i32 view_width, i32 view_height)
{
    PUSH_GRAPHICS_GROUP("gpu_render");

    glViewport(0, 0, r->width, r->height);
    glScissor(0, 0, r->width, r->height);
    glEnable(GL_BLEND);

    print_framebuffer_status();

    // TODO: Do less work when idling
    gpu_render_canvas(r, view_x, view_y, view_width, view_height);

    GLenum texture_target;
    if ( gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        texture_target = GL_TEXTURE_2D_MULTISAMPLE;
    } else {
        texture_target = GL_TEXTURE_2D;
    }

    // Use helper_texture as a place to do AA.

    // Blit the canvas to helper_texture

    glDisable(GL_DEPTH_TEST);

    PUSH_GRAPHICS_GROUP("blit to helper texture");
    if ( !gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                                  r->canvas_texture, 0);
        glBindTexture(texture_target, r->helper_texture);
        glCopyTexImage2D(texture_target, 0, GL_RGBA8, 0,0, r->width, r->height, 0);

        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                                  r->helper_texture, 0);
        glBindTexture(texture_target, r->canvas_texture);
    } else {
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target,
                                  r->helper_texture, 0);
        glBindTexture(texture_target, r->canvas_texture);

        gpu_fill_with_texture(r);
    }
    POP_GRAPHICS_GROUP();

    // Render GUI on top of helper_texture

    // Render color picker
    // TODO: Only render if view rect intersects picker rect
    if ( r->flags & RenderBackendFlags_GUI_VISIBLE ) {
        // Render picker
        gl::use_program(r->picker_program);
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

    PUSH_GRAPHICS_GROUP("postproc");
    if ( !gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        glBindFramebufferEXT(GL_FRAMEBUFFER, 0);

        // glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, r->helper_texture);

        gl::set_uniform_i(r->postproc_program, "u_canvas", 0);

        gl::use_program(r->postproc_program);

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
    else {  // Resolve
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, r->fbo);
        glBlitFramebufferEXT(0, 0, r->width, r->height,
                             0, 0, r->width, r->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    POP_GRAPHICS_GROUP();

    // Render outlines after doing AA.

    PUSH_GRAPHICS_GROUP("outlines");
    // Brush outline
    {
        gl::use_program(r->outline_program);
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
    if ( r->imm_flags & ImmediateFlag_RECT ) {
        // Update data if rect is not degenerate.
        // Draw outline.
        gl::use_program(r->exporter_program);
        GLint loc = glGetAttribLocation(r->exporter_program, "a_position");
        if ( loc>=0 && r->vbo_exporter > 0 ) {
            DEBUG_gl_validate_buffer(r->vbo_exporter);
            gl::vertex_attrib_v2f(r->exporter_program, "a_position", r->vbo_exporter);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->exporter_indices);

            glDrawElements(GL_TRIANGLES, r->exporter_indices_count, GL_UNSIGNED_SHORT, 0);
        }
    }
    POP_GRAPHICS_GROUP();  // outlines

    gl::use_program(0);
    POP_GRAPHICS_GROUP(); // gpu_render
}

void
gpu_render_to_buffer(Milton* milton, u8* buffer, i32 scale, i32 x, i32 y, i32 w, i32 h, f32 background_alpha)
{
    CanvasView saved_view = *milton->view;
    RenderBackend* r = milton->renderer;
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
    gpu_clip_strokes_and_update(&milton->root_arena, r, milton->view, milton->view->scale, milton->canvas->root_layer,
                                &milton->working_stroke, 0, 0, buf_w, buf_h);

    gpu_render_canvas(r, 0, 0, buf_w, buf_h, background_alpha);

    // Post processing
    if ( !gl::check_flags(GLHelperFlags_TEXTURE_MULTISAMPLE) ) {
        gl::use_program(r->postproc_program);
        glBindTexture(GL_TEXTURE_2D, r->canvas_texture);

        GLint loc = glGetAttribLocation(r->postproc_program, "a_position");
        if ( loc >= 0 ) {
            DEBUG_gl_validate_buffer(r->vbo_screen_quad);
            glBindBuffer(GL_ARRAY_BUFFER, r->vbo_screen_quad);
            glVertexAttribPointer((GLuint)loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray((GLuint)loc);

            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    } else {
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
                                r, milton->view, milton->view->scale, milton->canvas->root_layer,
                                &milton->working_stroke, 0, 0, r->width,
                                r->height);
    gpu_render(r, 0, 0, r->width, r->height);
}

void
gpu_release_data(RenderBackend* r)
{
    release(&r->clip_array);
}


void
imm_begin_frame(RenderBackend* r)
{
    r->imm_flags = 0;
}

void
imm_rect(RenderBackend* r, float left, float right, float top, float bottom, float line_width)
{
    if ( r->vbo_exporter == 0 ) {
        glGenBuffers(1, &r->vbo_exporter);
        mlt_assert(r->exporter_indices == 0);
        glGenBuffers(1, &r->exporter_indices);
    }

    float toparr[] = {
        // Top quad
        left, top - line_width/r->height,
        left, top + line_width/r->height,
        right, top + line_width/r->height,
        right, top - line_width/r->height,

        // Bottom quad
        left, bottom-line_width/r->height,
        left, bottom+line_width/r->height,
        right, bottom+line_width/r->height,
        right, bottom-line_width/r->height,

        // Left
        left-line_width/r->width, top,
        left-line_width/r->width, bottom,
        left+line_width/r->width, bottom,
        left+line_width/r->width, top,

        // Right
        right-line_width/r->width, top,
        right-line_width/r->width, bottom,
        right+line_width/r->width, bottom,
        right+line_width/r->width, top,
    };
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_exporter);
    DEBUG_gl_mark_buffer(r->vbo_exporter);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(toparr)*sizeof(*toparr), toparr, GL_DYNAMIC_DRAW);

    u16 indices[] = {
        // top
        0,1,2,
        2,3,0,

        // bottom
        4,5,6,
        6,7,4,

        // left
        8,9,10,
        10,11,8,

        // right
        12,13,14,
        14,15,12,
    };
    glBindBuffer(GL_ARRAY_BUFFER, r->exporter_indices);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(indices)*sizeof(*indices), indices, GL_STATIC_DRAW);

    r->exporter_indices_count = array_count(indices);

    r->imm_flags |= ImmediateFlag_RECT;
}

void
gpu_reset_stroke(RenderBackend* r, RenderHandle handle)
{
    RenderElement* re = get_render_element(handle);
    if (re) {
        re->count = 0;
    }
}
