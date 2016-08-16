// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "shaders.gen.h"

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

#pragma warning (push)
#pragma warning (disable : 4668)
#pragma warning (disable : 4200)
#define buffer struct
//#include "common.glsl"
//#include "stroke_raster.v.glsl"
#undef main
#define main fragmentShaderMain
#define texture2D(a,b) VEC4(0)
#define sampler2D int
//#include "stroke_raster.f.glsl"
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


// Forward declarations:
//
void milton_save_buffer_to_file(PATH_CHAR* fname, u8* buffer, i32 w, i32 h);  // Defined in persist.cc
// ====

#define PRESSURE_RESOLUTION (1<<20)
#define MAX_DEPTH_VALUE (1<<24)   // Strokes have MAX_DEPTH_VALUE different z values. 1/i for each i in [0, MAX_DEPTH_VALUE)
                                    // Also defined in stroke_raster.v.glsl
                                    //
                                    // NOTE: Using this technique means that the algorithm is not correct.
                                    //  There is a low probability that one stroke will cover another
                                    //  stroke with the same z value.

struct TextureUnitID
{
    GLenum opengl_id;
    GLint id;
};

static TextureUnitID g_texture_unit_layer = { GL_TEXTURE2, 2 };
static TextureUnitID g_texture_unit_canvas = { GL_TEXTURE1, 1 };

struct RenderData
{
    f32 viewport_limits[2];  // OpenGL limits to the framebuffer size.
    // TODO: Tiled rendering to get around the viewport limitation.

    GLuint stroke_program;
    GLuint quad_program;
    GLuint picker_program;
    GLuint layer_blend_program;
    GLuint ssaa_program;
    GLuint outline_program;
    GLuint flood_program;
    GLuint exporter_program;
#if MILTON_DEBUG
    GLuint simple_program;
#endif

    // VBO for the screen-covering quad.
    GLuint vbo_quad;
    GLuint vbo_quad_uv;  // TODO: Remove and use gl_FragCoord?

    GLuint vbo_picker;
    GLuint vbo_picker_norm;

    GLuint vbo_outline;
    GLuint vbo_outline_sizes;

    GLuint vbo_exporter[4]; // One for each line in rectangle

    GLuint layer_texture;
    GLuint canvas_texture;
    GLuint stencil_texture;

    GLuint fbo;

    i32 flags;  // RenderDataFlags enum

    DArray<RenderElement> render_elems;

    i32 width;
    i32 height;

    v3f background_color;
    i32 stroke_z;
};

enum RenderDataFlags
{
    RenderDataFlags_NONE = 0,

    RenderDataFlags_GUI_VISIBLE = 1<<0,
    RenderDataFlags_EXPORTING   = 1<<1,
};

enum RenderElementFlags
{
    RenderElementFlags_NONE = 0,

    RenderElementFlags_LAYER          = 1<<0,
    RenderElementFlags_FILLS          = 1<<1,  // Fills screen. TODO: render in tiles?
    RenderElementFlags_WORKING_STROKE = 1<<2,  // Valid for working stroke. Used to split the canvas into two buffers.
};

static void print_framebuffer_status()
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    char* msg = NULL;
    switch (status)
    {
        case GL_FRAMEBUFFER_COMPLETE:
        {
            // OK!
            break;
        }
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        {
            msg = "Incomplete Attachment";
            break;
        }
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        {
            msg = "Missing Attachment";
            break;
        }
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        {
            msg = "Incomplete Draw Buffer";
            break;
        }
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        {
            msg = "Incomplete Read Buffer";
            break;
        }
        case GL_FRAMEBUFFER_UNSUPPORTED:
        {
            msg = "Unsupported Framebuffer";
            break;
        }
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        {
            msg = "Incomplete Multisample";
            break;
        }
        default:
        {
            msg = "Unknown";
            break;
        }
    }

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        char warning[1024];
        snprintf(warning, "Framebuffer Error: %s", msg);
        milton_log("Warning %s\n", warning);
    }
}

void gpu_update_picker(RenderData* render_data, ColorPicker* picker)
{
    glUseProgram(render_data->picker_program);
    // Transform to [-1,1]
    v2f a = picker->data.a;
    v2f b = picker->data.b;
    v2f c = picker->data.c;
    // TODO include my own lambda impl.
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
        // Barycentric to cartesian
        f32 fa = hsv.s;
        f32 fb = 1 - hsv.v;
        f32 fc = 1 - fa - fb;

        v2f point = add2f(add2f((scale2f(picker->data.c,fa)), scale2f(picker->data.b,fb)), scale2f(picker->data.a,fc));
        // Move to [-1,1]^2
        point = transform(point);
        gl_set_uniform_vec2(render_data->picker_program, "u_triangle_point", 1, point.d);
    }
    v4f colors[5] = {};
    ColorButton* button = &picker->color_buttons;
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
        v2i screen_size = {render_data->width / SSAA_FACTOR, render_data->height / SSAA_FACTOR};
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
        glBufferData(GL_ARRAY_BUFFER, array_count(data)*sizeof(*data), data, GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_picker_norm);
        glBufferData(GL_ARRAY_BUFFER, array_count(norm)*sizeof(*norm), norm, GL_STATIC_DRAW);
    }
}

enum BrushOutlineEnum
{
    BrushOutline_NO_FILL = 1<<0,
    BrushOutline_FILL    = 1<<1,
};

void gpu_update_brush_outline(RenderData* render_data, i32 cx, i32 cy, i32 radius,
                              BrushOutlineEnum outline_enum = BrushOutline_NO_FILL, v4f color = {})
{
    if (render_data->vbo_outline == 0)
    {
        mlt_assert(render_data->vbo_outline_sizes == 0);
        glGenBuffers(1, &render_data->vbo_outline);
        glGenBuffers(1, &render_data->vbo_outline_sizes);
    }
    mlt_assert(render_data->vbo_outline_sizes != 0);

    radius*=SSAA_FACTOR;

    float radius_plus_girth = radius + 4.0f; // Girth defined in outline.f.glsl


    // Normalized to [-1,1]
    GLfloat data[] =
    {
        2*((cx-radius_plus_girth) / (render_data->width))-1,  -2*((cy-radius_plus_girth) / (render_data->height))+1,
        2*((cx-radius_plus_girth) / (render_data->width))-1,  -2*((cy+radius_plus_girth) / (render_data->height))+1,
        2*((cx+radius_plus_girth) / (render_data->width))-1,  -2*((cy+radius_plus_girth) / (render_data->height))+1,
        2*((cx+radius_plus_girth) / (render_data->width))-1,  -2*((cy-radius_plus_girth) / (render_data->height))+1,
    };

    GLfloat sizes[] =
    {
        -radius_plus_girth, -radius_plus_girth,
        -radius_plus_girth,  radius_plus_girth,
         radius_plus_girth,  radius_plus_girth,
         radius_plus_girth, -radius_plus_girth,
    };

    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_outline_sizes);
    glBufferData(GL_ARRAY_BUFFER, array_count(sizes)*sizeof(*sizes), sizes, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_outline);
    GLCHK( glBufferData(GL_ARRAY_BUFFER, array_count(data)*sizeof(*data), data, GL_DYNAMIC_DRAW) );

    gl_set_uniform_i(render_data->outline_program, "u_radius", radius);  // Constant 5 the same as in shader outline.f.glsl
    if (outline_enum == BrushOutline_FILL)
    {
        gl_set_uniform_i(render_data->outline_program, "u_fill", true);
        gl_set_uniform_vec4(render_data->outline_program, "u_color", 1, color.d);
    }
    else if (outline_enum == BrushOutline_NO_FILL)
    {
        gl_set_uniform_i(render_data->outline_program, "u_fill", false);
    }
}

b32 render_element_is_layer(RenderElement* render_element)
{
    b32 result = (render_element->flags & RenderElementFlags_LAYER);
    return result;
}

b32 render_element_fills_screen(RenderElement* render_element)
{
    b32 result = (render_element->flags & RenderElementFlags_FILLS);
    return result;
}

b32 render_element_is_working_stroke(RenderElement* render_element)
{
    b32 result = (render_element->flags & RenderElementFlags_WORKING_STROKE);
    return result;
}

b32 gpu_init(RenderData* render_data, CanvasView* view, ColorPicker* picker, i32 render_data_flags)
{
    {
        GLfloat viewport_dims[2] = {};
        glGetFloatv(GL_MAX_VIEWPORT_DIMS, viewport_dims);
        milton_log("Maximum viewport dimensions, %fx%f\n", viewport_dims[0], viewport_dims[1]);
        render_data->viewport_limits[0] = viewport_dims[0];
        render_data->viewport_limits[1] = viewport_dims[1];
    }

    // Check stroke_z range
    //render_data->stroke_z = (1<<24) - 500;

    //mlt_assert(PRESSURE_RESOLUTION == PRESSURE_RESOLUTION_GL);
    // TODO: Handle this. New MLT version?
    // mlt_assert(STROKE_MAX_POINTS == STROKE_MAX_POINTS_GL);
    glEnable(GL_SCISSOR_TEST);

    bool result = true;

    render_data->flags = render_data_flags;

#if USE_3_2_CONTEXT
    // Assume our context is 3.0+
    // Create a single VAO and bind it.
    GLuint proxy_vao = 0;
    GLCHK( glGenVertexArrays(1, &proxy_vao) );
    GLCHK( glBindVertexArray(proxy_vao) );
#endif

    // Load shader into opengl.
    {
        GLuint objs[2];

        objs[0] = gl_compile_shader(g_stroke_raster_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_stroke_raster_f, GL_FRAGMENT_SHADER);

        render_data->stroke_program = glCreateProgram();

        gl_link_program(render_data->stroke_program, objs, array_count(objs));

        GLCHK( glUseProgram(render_data->stroke_program) );
        gl_set_uniform_i(render_data->stroke_program, "u_canvas", g_texture_unit_canvas.id);
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

        float u = 1.0f;
        GLfloat uv_data[] =
        {
            0,0,
            0,u,
            u,u,
            u,0,
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
    {  // Color picker program
        render_data->picker_program = glCreateProgram();
        GLuint objs[2] = {};

        // g_picker_* generated by shadergen.cc
        objs[0] = gl_compile_shader(g_picker_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_picker_f, GL_FRAGMENT_SHADER);
        gl_link_program(render_data->picker_program, objs, array_count(objs));
    }
    {  // Layer blend program
        render_data->layer_blend_program = glCreateProgram();
        GLuint objs[2] = {};

        objs[0] = gl_compile_shader(g_layer_blend_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_layer_blend_f, GL_FRAGMENT_SHADER);
        gl_link_program(render_data->layer_blend_program, objs, array_count(objs));
        gl_set_uniform_i(render_data->layer_blend_program, "u_canvas", g_texture_unit_canvas.id);
    }
    {  // SSAA resolve program
        render_data->ssaa_program = glCreateProgram();

        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_ssaa_resolve_f, GL_FRAGMENT_SHADER);

        gl_link_program(render_data->ssaa_program, objs, array_count(objs));

        gl_set_uniform_i(render_data->ssaa_program, "u_canvas", g_texture_unit_canvas.id);
    }
    {  // Brush outline program
        render_data->outline_program = glCreateProgram();
        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_outline_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_outline_f, GL_FRAGMENT_SHADER);

        gl_link_program(render_data->outline_program, objs, array_count(objs));
    }
    {  // Flood fill program
        render_data->flood_program = glCreateProgram();

        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_flood_f, GL_FRAGMENT_SHADER);

        gl_link_program(render_data->flood_program, objs, array_count(objs));
        gl_set_uniform_i(render_data->flood_program, "u_canvas", g_texture_unit_canvas.id);
    }
    {  // Exporter program
        render_data->exporter_program = glCreateProgram();

        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_exporter_f, GL_FRAGMENT_SHADER);

        gl_link_program(render_data->exporter_program, objs, array_count(objs));
        gl_set_uniform_i(render_data->exporter_program, "u_canvas", g_texture_unit_layer.id);
    }
    {  // Simple program
        render_data->simple_program = glCreateProgram();

        GLuint objs[2] = {};
        objs[0] = gl_compile_shader(g_simple_v, GL_VERTEX_SHADER);
        objs[1] = gl_compile_shader(g_simple_f, GL_FRAGMENT_SHADER);

        gl_link_program(render_data->simple_program, objs, array_count(objs));
    }

    // Framebuffer object for canvas. Layer buffer
    {
        glActiveTexture(g_texture_unit_layer.opengl_id);
        GLCHK (glGenTextures(1, &render_data->layer_texture));
        glBindTexture(GL_TEXTURE_2D, render_data->layer_texture);

        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

        glTexImage2D(GL_TEXTURE_2D,
                     0, GL_RGBA,
                     view->screen_size.w, view->screen_size.h,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        glBindTexture(GL_TEXTURE_2D, render_data->layer_texture);

        glBindTexture(GL_TEXTURE_2D, 0);

        glActiveTexture(g_texture_unit_canvas.opengl_id);
        GLCHK (glGenTextures(1, &render_data->canvas_texture));
        glBindTexture(GL_TEXTURE_2D, render_data->canvas_texture);

        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

        glTexImage2D(GL_TEXTURE_2D,
                     0, GL_RGBA,
                     view->screen_size.w, view->screen_size.h,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);


        glBindTexture(GL_TEXTURE_2D, 0);

        glGenTextures(1, &render_data->stencil_texture);
        glBindTexture(GL_TEXTURE_2D, render_data->stencil_texture);


        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        /* glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY); */
        /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE); */
        /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL); */
        GLCHK( glTexImage2D(GL_TEXTURE_2D, 0,
                            /*internalFormat, num of components*/GL_DEPTH24_STENCIL8,
                            view->screen_size.w, view->screen_size.h,
                            /*border*/0, /*pixel_data_format*/GL_DEPTH_STENCIL,
                            /*component type*/GL_UNSIGNED_INT_24_8,
                            NULL) );


        glBindTexture(GL_TEXTURE_2D, 0);
        /* GLuint rbo = 0; */
        /* glGenRenderbuffers(1, &rbo); */
        /* glBindRenderbuffer(GL_RENDERBUFFER, rbo); */
        /* glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, view->screen_size.w, view->screen_size.h); */
        //GLCHK( glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MUL, stencil_texture, 0) );

#if 0
        int depth_size;
        GLCHK( glGetIntegerv(GL_DEPTH_BITS, &depth_size) );
        int stencil_size;
        GLCHK( glGetIntegerv(GL_STENCIL_BITS, &stencil_size) );
#endif

        {
            GLuint fbo = 0;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            GLCHK( glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                        render_data->layer_texture, 0) );
            GLCHK( glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                        render_data->stencil_texture, 0) );
            render_data->fbo = fbo;
            print_framebuffer_status();
        }
        GLCHK( glBindFramebuffer(GL_FRAMEBUFFER, 0) );
    }
    // VBO for picker
    glGenBuffers(1, &render_data->vbo_picker);
    glGenBuffers(1, &render_data->vbo_picker_norm);

    // Call gpu_update_picker() to initialize the color picker
    gpu_update_picker(render_data, picker);
    return result;
}

void gpu_resize(RenderData* render_data, CanvasView* view)
{
    render_data->width = view->screen_size.w;
    render_data->height = view->screen_size.h;

    i32 tex_w = render_data->width;
    i32 tex_h = render_data->height;
    // Create canvas texture

    glActiveTexture(g_texture_unit_canvas.opengl_id);
    GLCHK (glBindTexture(GL_TEXTURE_2D, render_data->canvas_texture));
    GLCHK (glTexImage2D(GL_TEXTURE_2D, 0, /*internalFormat, num of components*/GL_RGBA8,
                        tex_w, tex_h,
                        /*border*/0, /*pixel_data_format*/GL_BGRA,
                        /*component type*/GL_UNSIGNED_BYTE, NULL));
    glActiveTexture(g_texture_unit_layer.opengl_id);
    GLCHK (glBindTexture(GL_TEXTURE_2D, render_data->layer_texture));
    GLCHK (glTexImage2D(GL_TEXTURE_2D, 0, /*internalFormat, num of components*/GL_RGBA8,
                        tex_w, tex_h,
                        /*border*/0, /*pixel_data_format*/GL_BGRA,
                        /*component type*/GL_UNSIGNED_BYTE, NULL));


    glBindTexture(GL_TEXTURE_2D, render_data->stencil_texture);
    GLCHK( glTexImage2D(GL_TEXTURE_2D, 0,
                        /*internalFormat, num of components*/GL_DEPTH24_STENCIL8,
                        tex_w, tex_h,
                        /*border*/0, /*pixel_data_format*/GL_DEPTH_STENCIL,
                        /*component type*/GL_UNSIGNED_INT_24_8,
                        NULL) );
}

i32 g_scale = -1;

void gpu_update_scale(RenderData* render_data, i32 scale)
{
#if MILTON_DEBUG // set the shader values in C++
    // Shader
    // u_scale = scale;
#endif
    gl_set_uniform_i(render_data->stroke_program, "u_scale", scale);
    g_scale = scale;
}

void gpu_update_export_rect(RenderData* render_data, Exporter* exporter)
{
    if (render_data->vbo_exporter[0] == 0)
    {
        glGenBuffers(4, render_data->vbo_exporter);
    }

    i32 x = min(exporter->pivot.x, exporter->needle.x);
    i32 y = min(exporter->pivot.y, exporter->needle.y);
    i32 w = abs(exporter->pivot.x - exporter->needle.x);
    i32 h = abs(exporter->pivot.y - exporter->needle.y);

    // Normalize to [-1,1]^2
    float normalized_rect[] =
    {
        2*((GLfloat)    x/(render_data->width/SSAA_FACTOR))-1, -(2*((GLfloat)y    /(render_data->height/SSAA_FACTOR))-1),
        2*((GLfloat)    x/(render_data->width/SSAA_FACTOR))-1, -(2*((GLfloat)(y+h)/(render_data->height/SSAA_FACTOR))-1),
        2*((GLfloat)(x+w)/(render_data->width/SSAA_FACTOR))-1, -(2*((GLfloat)(y+h)/(render_data->height/SSAA_FACTOR))-1),
        2*((GLfloat)(x+w)/(render_data->width/SSAA_FACTOR))-1, -(2*((GLfloat)y    /(render_data->height/SSAA_FACTOR))-1),
    };

    float px = 2.0f;
    float line_width = px / (render_data->height/SSAA_FACTOR);
    float aspect = render_data->width / (float)render_data->height;

    float top[] =
    {
        normalized_rect[0], normalized_rect[1],
        normalized_rect[2], normalized_rect[1]+line_width,
        normalized_rect[4], normalized_rect[1]+line_width,
        normalized_rect[6], normalized_rect[1],
    };
    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_exporter[0]);
    GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(top)*sizeof(*top), top, GL_DYNAMIC_DRAW) );

    float bottom[] =
    {
        normalized_rect[0], normalized_rect[3]-line_width,
        normalized_rect[2], normalized_rect[3],
        normalized_rect[4], normalized_rect[3],
        normalized_rect[6], normalized_rect[3]-line_width,
    };
    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_exporter[1]);
    GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(bottom)*sizeof(*bottom), bottom, GL_DYNAMIC_DRAW) );


    line_width = px / (render_data->width/SSAA_FACTOR);

    float right[] =
    {
        normalized_rect[4]-line_width, normalized_rect[1],
        normalized_rect[4]-line_width, normalized_rect[3],
        normalized_rect[4], normalized_rect[5],
        normalized_rect[4], normalized_rect[7],
    };
    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_exporter[2]);
    GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(right)*sizeof(*right), right, GL_DYNAMIC_DRAW) );

    float left[] =
    {
        normalized_rect[0], normalized_rect[1],
        normalized_rect[0], normalized_rect[3],
        normalized_rect[0]+line_width, normalized_rect[5],
        normalized_rect[0]+line_width, normalized_rect[7],
    };
    glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_exporter[3]);
    GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)array_count(left)*sizeof(*left), left, GL_DYNAMIC_DRAW) );

}

static void gpu_set_background(RenderData* render_data, v3f background_color)
{
#if MILTON_DEBUG
    // SHADER
    // for(int i=0;i<3;++i) u_background_color.d[i] = background_color.d[i];
#endif
    gl_set_uniform_vec3(render_data->stroke_program, "u_background_color", 1, background_color.d);

    render_data->background_color = background_color;
}

void set_screen_size(RenderData* render_data, float* fscreen)
{
    gl_set_uniform_vec2(render_data->stroke_program, "u_screen_size", 1, fscreen);
    gl_set_uniform_vec2(render_data->ssaa_program, "u_screen_size", 1, fscreen);
    gl_set_uniform_vec2(render_data->layer_blend_program, "u_screen_size", 1, fscreen);
    gl_set_uniform_vec2(render_data->flood_program, "u_screen_size", 1, fscreen);

    // Post SSAA
    float fscreen2[2] = {fscreen[0]/SSAA_FACTOR, fscreen[1]/SSAA_FACTOR};
    gl_set_uniform_vec2(render_data->exporter_program, "u_screen_size", 1, fscreen2);
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

    auto center = divide2i(view->screen_center, 1);
    auto pan = divide2i(view->pan_vector, 1);
    gl_set_uniform_vec2i(render_data->stroke_program, "u_pan_vector", 1, pan.d);
    gl_set_uniform_vec2i(render_data->stroke_program, "u_screen_center", 1, center.d);
    gl_set_uniform_i(render_data->stroke_program, "u_scale", view->scale);
    float fscreen[] = { (float)view->screen_size.x, (float)view->screen_size.y };
    set_screen_size(render_data, fscreen);
}

void gpu_clip_strokes(RenderData* render_data,
                      CanvasView* view,
                      Layer* root_layer, Stroke* working_stroke)
{
    auto *render_elements = &render_data->render_elems;

    RenderElement layer_element = {};
    layer_element.flags |= RenderElementFlags_LAYER;

    reset(render_elements);
    for(Layer* l = root_layer;
        l != NULL;
        l = l->next)
    {
        if (!(l->flags & LayerFlags_VISIBLE))
        {
            // Skip invisible layers.
            continue;
        }
        for (u64 i = 0; i <= l->strokes.count; ++i)
        {
            // Only push the working stroke when the next layer is NULL, which means that we are at the topmost layer.
            if (i == l->strokes.count && l->id == working_stroke->layer_id)
            {
                if (working_stroke->num_points > 0)
                {
                    push(render_elements, working_stroke->render_element);
                }
            }
            else if (i < l->strokes.count)
            {
                Stroke* s = &l->strokes.data[i];
                Rect bounds = s->bounding_rect;
                bounds.top_left = canvas_to_raster(view, bounds.top_left);
                bounds.bot_right = canvas_to_raster(view, bounds.bot_right);
                b32 is_outside = bounds.left > render_data->width || bounds.right < 0 ||
                                bounds.top > render_data->height || bounds.bottom < 0;
                i32 area = (bounds.right-bounds.left) * (bounds.bottom-bounds.top);

                s->render_element.flags = RenderElementFlags_NONE;
                if (bounds.left <= 0 && bounds.right >= render_data->width &&
                    bounds.top <= 0 && bounds.bottom >= render_data->height)
                {
                    // Check that the screen is inside the stroke.
                    auto np = s->num_points;
                    if (np > 1)
                    {
                        for (i32 pi = 0; pi < np-1; ++pi)
                        {
                            v2i a = canvas_to_raster(view, s->points[pi]);
                            v2i b = canvas_to_raster(view, s->points[pi+1]);

                            f32 pa = s->pressures[pi];
                            f32 pb = s->pressures[pi+1];

                            f32 radius = s->brush.radius/(float)view->scale * SSAA_FACTOR * min(pa, pb);

                            float left = min(a.x, b.x) - radius;
                            float right = max(a.x, b.x) + radius;
                            float top = min(a.y, b.y) - radius;
                            float bottom = max(a.y, b.y) + radius;

                            if (left <= 0 && right >= render_data->width &&
                                top <= 0 && bottom >= render_data->height)
                            {
                                s->render_element.flags |= RenderElementFlags_FILLS;
                                break;
                            }
                        }
                    }
                }
                if (!is_outside && area!=0)
                {
                    push(render_elements, s->render_element);
                }
            }
        }

        push(render_elements, layer_element);
    }
}

void resolve_SSAA(RenderData* render_data, GLuint draw_framebuffer)
{
    if (SSAA_FACTOR == 2 || SSAA_FACTOR == 1)
    {
        GLCHK( glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer) );
        glBindFramebuffer(GL_READ_FRAMEBUFFER, render_data->fbo);
        GLCHK( glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_data->canvas_texture, 0) );
        GLCHK( glBlitFramebuffer(0, 0, render_data->width, render_data->height,
                                 0, 0, render_data->width/SSAA_FACTOR, render_data->height/SSAA_FACTOR,
                                 GL_COLOR_BUFFER_BIT, GL_LINEAR) );
    }
    else if (SSAA_FACTOR == 4)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(render_data->ssaa_program);
        {
            GLint loc = glGetAttribLocation(render_data->ssaa_program, "a_position");
            if (loc >= 0)
            {
                glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_quad);
                GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                             /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                             /*stride*/0, /*ptr*/0));
                glEnableVertexAttribArray((GLuint)loc);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
        }
    }
    else
    {
        milton_die_gracefully("unsupported SSAA_FACTOR");
    }


}
// TODO: Measure memory consumption of glBufferData and their ilk
enum CookStrokeOpt
{
    CookStroke_NEW                   = 0,
    CookStroke_UPDATE_WORKING_STROKE = 1,
};
void gpu_cook_stroke(Arena* arena, RenderData* render_data, Stroke* stroke, CookStrokeOpt cook_option = CookStroke_NEW)
{
    render_data->stroke_z = (render_data->stroke_z + 1) % MAX_DEPTH_VALUE;
    const i32 stroke_z = render_data->stroke_z;

    if (cook_option == CookStroke_NEW && stroke->render_element.vbo_stroke != 0)
    {
        // We already have our data cooked
        mlt_assert(stroke->render_element.vbo_pointa != 0);
        mlt_assert(stroke->render_element.vbo_pointb != 0);
    }
    else
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
        auto npoints = stroke->num_points;
        if (npoints == 1)
        {
            // TODO: handle special case...
            // So uncommon that adding an extra degenerate point might make sense,
            // if the algorithm can handle it

        }
        else if (npoints > 1)
        {
            GLCHK( glUseProgram(render_data->stroke_program) );

            // 3 (triangle) *
            // 2 (two per segment) *
            // N-1 (segments per stroke)
            const size_t count_bounds = 3*2*((size_t)npoints-1);

            // 6 (3 * 2 from count_bounds)
            // N-1 (num segments)
            const size_t count_points = 6*((size_t)npoints-1);

            v3i* bounds;
            v3i* apoints;
            v3i* bpoints;
            Arena scratch_arena = arena_push(arena, count_bounds*sizeof(decltype(*bounds))
             + 2*count_points*sizeof(decltype(*apoints)));

            bounds  = arena_alloc_array(&scratch_arena, count_bounds, v3i);
            apoints = arena_alloc_array(&scratch_arena, count_bounds, v3i);
            bpoints = arena_alloc_array(&scratch_arena, count_bounds, v3i);

            size_t bounds_i = 0;
            size_t apoints_i = 0;
            size_t bpoints_i = 0;
            for (i64 i=0; i < npoints-1; ++i)
            {
                v2i point_i = stroke->points[i];
                v2i point_j = stroke->points[i+1];

                Brush brush = stroke->brush;
                float radius_i = stroke->pressures[i]*brush.radius*SSAA_FACTOR;
                float radius_j = stroke->pressures[i+1]*brush.radius*SSAA_FACTOR;

                i32 min_x = min(point_i.x-radius_i, point_j.x-radius_j);
                i32 min_y = min(point_i.y-radius_i, point_j.y-radius_j);

                i32 max_x = max(point_i.x+radius_i, point_j.x+radius_j);
                i32 max_y = max(point_i.y+radius_i, point_j.y+radius_j);

                // Bounding rect
                //  Counter-clockwise
                //  TODO: Use triangle strips and flat shading to reduce redundant data
                bounds[bounds_i++] = { min_x, min_y, stroke_z };
                bounds[bounds_i++] = { min_x, max_y, stroke_z };
                bounds[bounds_i++] = { max_x, max_y, stroke_z };

                bounds[bounds_i++] = { max_x, max_y, stroke_z };
                bounds[bounds_i++] = { min_x, min_y, stroke_z };
                bounds[bounds_i++] = { max_x, min_y, stroke_z };

                // Pressures are in (0,1] but we need to encode them as integers.
                i32 pressure_a = (i32)(stroke->pressures[i] * (float)(PRESSURE_RESOLUTION));
                i32 pressure_b = (i32)(stroke->pressures[i+1] * (float)(PRESSURE_RESOLUTION));

                // one for every point in the triangle.
                for (int repeat = 0; repeat < 6; ++repeat)
                {
                    apoints[apoints_i++] = { point_i.x, point_i.y, pressure_a };
                    bpoints[bpoints_i++] = { point_j.x, point_j.y, pressure_b };
                }
            }

            mlt_assert(bounds_i == count_bounds);

            // TODO: check for GL_OUT_OF_MEMORY

            GLuint vbo_stroke = 0;
            GLuint vbo_pointa = 0;
            GLuint vbo_pointb = 0;


            GLenum hint = GL_STATIC_DRAW;
            if (cook_option == CookStroke_UPDATE_WORKING_STROKE)
            {
                hint = GL_DYNAMIC_DRAW;
            }
            if (stroke->render_element.vbo_stroke == 0)  // Cooking the stroke for the first time.
            {
                glGenBuffers(1, &vbo_stroke);
                glGenBuffers(1, &vbo_pointa);
                glGenBuffers(1, &vbo_pointb);
                glBindBuffer(GL_ARRAY_BUFFER, vbo_stroke);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bounds))), bounds, hint) );
                glBindBuffer(GL_ARRAY_BUFFER, vbo_pointa);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*apoints))), apoints, hint) );
                glBindBuffer(GL_ARRAY_BUFFER, vbo_pointb);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bpoints))), bpoints, hint) );
            }
            else  // Updating the working stroke
            {
                vbo_stroke = stroke->render_element.vbo_stroke;
                vbo_pointa = stroke->render_element.vbo_pointa;
                vbo_pointb = stroke->render_element.vbo_pointb;

                glBindBuffer(GL_ARRAY_BUFFER, vbo_stroke);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bounds))), NULL, hint) );
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bounds))), bounds, hint) );
                glBindBuffer(GL_ARRAY_BUFFER, vbo_pointa);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*apoints))), NULL, hint) );
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*apoints))), apoints, hint) );
                glBindBuffer(GL_ARRAY_BUFFER, vbo_pointb);
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bpoints))), NULL, hint) );
                GLCHK( glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(bounds_i*sizeof(decltype(*bpoints))), bpoints, hint) );
            }

            GLuint ubo = 0;

            RenderElement re = stroke->render_element;
            re.vbo_stroke = vbo_stroke;
            re.vbo_pointa = vbo_pointa;
            re.vbo_pointb = vbo_pointb;
            re.count = (i64)bounds_i;
            re.color = { stroke->brush.color.r, stroke->brush.color.g, stroke->brush.color.b, stroke->brush.color.a };
            re.radius = stroke->brush.radius*SSAA_FACTOR;
            mlt_assert(re.count > 1);

            stroke->render_element = re;

            arena_pop(&scratch_arena);
        }
    }
}

void gpu_free_strokes(Stroke* strokes, i64 count)
{
    for (i64 i = 0; i < count; ++i)
    {
        Stroke* s = strokes + i;
        RenderElement* re = &s->render_element;
        if (re->vbo_stroke != 0)
        {
            mlt_assert(re->vbo_pointa != 0);
            mlt_assert(re->vbo_pointb != 0);
            glDeleteBuffers(1, &re->vbo_stroke);
            glDeleteBuffers(1, &re->vbo_pointa);
            glDeleteBuffers(1, &re->vbo_pointb);
            re->vbo_stroke = 0;
            re->vbo_pointa = 0;
            re->vbo_pointb = 0;
        }
    }
}

void gpu_free_strokes(MiltonState* milton_state)
{
    if (milton_state->root_layer != NULL &&
        milton_state->root_layer->strokes.data[0].render_element.vbo_stroke!=0)
    {
        for(Layer* l = milton_state->root_layer;
            l != NULL;
            l = l->next)
        {
            gpu_free_strokes(l->strokes.data, (i64)l->strokes.count);
        }

    }
}

void gpu_render_canvas(RenderData* render_data, i32 view_x, i32 view_y, i32 view_width, i32 view_height)
{
    i32 x = view_x;
    i32 y = view_y;
    i32 w = view_width;
    i32 h = view_height;
    glScissor(x,y, w, h);

    GLCHK( glActiveTexture(g_texture_unit_layer.opengl_id) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, render_data->layer_texture) );
    glClearColor(render_data->background_color.r,
                 render_data->background_color.g,
                 render_data->background_color.b,
                 1.0f);

    glBindFramebuffer(GL_FRAMEBUFFER, render_data->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_data->canvas_texture, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    GLCHK( glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_data->layer_texture, 0) );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_NOTEQUAL);

    GLCHK( glUseProgram(render_data->stroke_program) );
    GLint loc = glGetAttribLocation(render_data->stroke_program, "a_position");
    GLint loc_a = glGetAttribLocation(render_data->stroke_program, "a_pointa");
    GLint loc_b = glGetAttribLocation(render_data->stroke_program, "a_pointb");
    if (loc >= 0)
    {

        //for (i64 i = (i64)render_data->render_elems.count-1; i>=0; --i)
        for (i64 i = 0 ; i <(i64)render_data->render_elems.count; i++)
        {
            RenderElement* re = &render_data->render_elems.data[i];

            if (render_element_is_layer(re))
            {
                // Layer render element.
                // The current frambuffer is layer_texture. We copy its contents to the canvas_texture
                glBindTexture(GL_TEXTURE_2D, render_data->canvas_texture);
                GLCHK( glCopyTexImage2D(GL_TEXTURE_2D, /*lod*/0, GL_RGBA8, 0,0, render_data->width,render_data->height, /*border*/0) );
            }
            else
            {
                if (render_element_fills_screen(re))
                {
                    glDisable(GL_DEPTH_TEST);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    gl_set_uniform_vec4(render_data->flood_program, "u_brush_color", 1, re->color.d);
                    // Fill screen path
                    glUseProgram(render_data->flood_program);
                    GLint p_loc = glGetAttribLocation(render_data->flood_program, "a_position");
                    if (p_loc >= 0)
                    {
                        GLCHK( glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_quad) );
                        GLCHK( glVertexAttribPointer(/*attrib p_location*/(GLuint)p_loc,
                                                     /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                                     /*stride*/0, /*ptr*/0));
                        glEnableVertexAttribArray((GLuint)p_loc);
                        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                    }
                    glUseProgram(render_data->stroke_program);
                    glEnable(GL_DEPTH_TEST);
                    glDisable(GL_BLEND);
                }
                else
                {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    i64 count = re->count;

                    // TODO. Only set these uniforms when both are different from the ones in use.
                    gl_set_uniform_vec4(render_data->stroke_program, "u_brush_color", 1, re->color.d);
                    gl_set_uniform_i(render_data->stroke_program, "u_radius", re->radius);

                    if (loc_a >=0)
                    {
                        GLCHK( glBindBuffer(GL_ARRAY_BUFFER, re->vbo_pointa) );
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
                        GLCHK( glBindBuffer(GL_ARRAY_BUFFER, re->vbo_pointb) );
#if 0
                        GLCHK( glVertexAttribIPointer(/*attrib location*/(GLuint)loc_b,
                                                      /*size*/3, GL_INT,
                                                      /*stride*/0, /*ptr*/0));
#else
                        GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc_b,
                                                     /*size*/3, GL_INT, /*normalize*/GL_FALSE,
                                                     /*stride11,059,200*/0, /*ptr*/0));
#endif
                        GLCHK( glEnableVertexAttribArray((GLuint)loc_b) );
                    }


                    GLCHK( glBindBuffer(GL_ARRAY_BUFFER, re->vbo_stroke) );
                    GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                                 /*size*/3, GL_INT, /*normalize*/GL_FALSE,
                                                 /*stride*/0, /*ptr*/0));
                    GLCHK( glEnableVertexAttribArray((GLuint)loc) );

                    GLCHK( glUseProgram(render_data->stroke_program) );
                    GLCHK( glDrawArrays(GL_TRIANGLES, 0, count) );

                    glDisable(GL_BLEND);  // TODO: everyone is using blend?
                }
            }
        }
    }
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, render_data->width, render_data->height);
    glScissor(0, 0, render_data->width, render_data->height);
}


void gpu_render(RenderData* render_data,  i32 view_x, i32 view_y, i32 view_width, i32 view_height)
{
    glClearDepth(0.0f);
    glViewport(0, 0, render_data->width, render_data->height);
    glScissor(0, 0, render_data->width, render_data->height);
    GLCHK( glBindFramebuffer(GL_FRAMEBUFFER, render_data->fbo) );
    print_framebuffer_status();

    gpu_render_canvas(render_data, view_x, view_y, view_width, view_height);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0/SSAA_FACTOR,0/SSAA_FACTOR, render_data->width/SSAA_FACTOR, render_data->height/SSAA_FACTOR);
    glScissor(0/SSAA_FACTOR,0/SSAA_FACTOR, render_data->width/SSAA_FACTOR, render_data->height/SSAA_FACTOR);

    // Resolve MSAA
    resolve_SSAA(render_data, 0/*default framebuffer*/);

    //GLCHK( glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_data->canvas_texture, 0) );
    glEnable(GL_BLEND);
    if (render_data->flags & RenderDataFlags_GUI_VISIBLE)
    {
        // Render picker

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(render_data->picker_program);
        GLint loc = glGetAttribLocation(render_data->picker_program, "a_position");

        if (loc >= 0)
        {
            GLCHK( glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_picker) );
            GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                         /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                         /*stride*/0, /*ptr*/0));
            glEnableVertexAttribArray((GLuint)loc);
            GLint loc_norm = glGetAttribLocation(render_data->picker_program, "a_norm");
            if (loc_norm >= 0)
            {
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
        if (loc >= 0)
        {
            glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_outline);

            GLCHK( glVertexAttribPointer(/*attrib location*/(GLuint)loc,
                                         /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                         /*stride*/0, /*ptr*/0));
            glEnableVertexAttribArray((GLuint)loc);
            GLint loc_s = glGetAttribLocation(render_data->outline_program, "a_sizes");
            if (loc_s >= 0)
            {
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
    if (render_data->flags & RenderDataFlags_EXPORTING)
    {
        glDisable(GL_DEPTH_TEST);
        // Update data if rect is not degenerate.
        // Draw outline.
        glUseProgram(render_data->exporter_program);
        GLint loc = glGetAttribLocation(render_data->exporter_program, "a_position");
        if (loc>=0 && render_data->vbo_exporter>0)
        {
            for (int vbo_i = 0; vbo_i < 4; ++vbo_i)
            {
                glBindBuffer(GL_ARRAY_BUFFER, render_data->vbo_exporter[vbo_i]);
                glVertexAttribPointer((GLuint)loc, 2, GL_FLOAT, GL_FALSE, 0,0);
                glEnableVertexAttribArray((GLuint)loc);

                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
        }
        glEnable(GL_DEPTH_TEST);
    }


    GLCHK (glUseProgram(0));
}

void gpu_export(MiltonState* milton_state)
{
    CanvasView saved_view = *milton_state->view;
    RenderData* render_data = milton_state->render_data;
    CanvasView* view = milton_state->view;

    i32 saved_width = render_data->width;
    i32 saved_height = render_data->height;
    GLuint saved_fbo = render_data->fbo;


    int scale = 2;
    i32 x = 0;
    i32 y = 0;
    i32 w = render_data->width;
    i32 h = render_data->height;
    i32 buf_w = render_data->width * scale;
    i32 buf_h = render_data->height * scale;


    v2i center = divide2i(milton_state->view->screen_size, 2);
    v2i pan_delta = sub2i(center, v2i{x + (w/2), y + (h/2)}) ;

    milton_state->view->pan_vector = add2i(milton_state->view->pan_vector, scale2i(pan_delta, milton_state->view->scale));

    milton_state->view->screen_size = v2i{ buf_w, buf_h };
    render_data->width = buf_w;
    render_data->height = buf_h;

    milton_state->view->screen_center = divide2i(milton_state->view->screen_size, 2);
    if ( scale > 1 )
    {
        milton_state->view->scale = (i32)ceill(((f32)milton_state->view->scale / (f32)scale));
    }

    gpu_resize(render_data, view);
    gpu_set_canvas(render_data, view);

    u8* data = (u8*)mlt_malloc((render_data->width/SSAA_FACTOR) * (render_data->height/SSAA_FACTOR) * sizeof(i32));

    GLsizei sizes[2][2] =
    {
        { buf_w, buf_h },
        { buf_w/SSAA_FACTOR, buf_h/SSAA_FACTOR },
    };
    GLuint export_fbo = 0;
    GLuint export_texture = 0;  // The resized canvas.
    GLuint buffer_texture = 0;  // The final, SSAA-resolved texture.

    glGenTextures(1, &buffer_texture);
    GLCHK (glGenTextures(1, &export_texture) );


    for (int tex_i=0; tex_i < 2; ++tex_i)
    {
        glActiveTexture(GL_TEXTURE0);
        switch(tex_i)
        {
        case 0:
            glBindTexture(GL_TEXTURE_2D, export_texture);
            break;
        case 1:
            glBindTexture(GL_TEXTURE_2D, buffer_texture);
            break;
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLsizei tex_w = sizes[tex_i][0];
        GLsizei tex_h = sizes[tex_i][1];
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }

    {
        glGenFramebuffers(1, &export_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, export_fbo);
        GLCHK( glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer_texture, 0) );
        print_framebuffer_status();
    }

    // TODO: Check for out-of-memory errors.

    mlt_assert(buf_w == render_data->width);
    mlt_assert(buf_h == render_data->height);

    glViewport(0, 0, buf_w, buf_h);
    gpu_clip_strokes(render_data, milton_state->view, milton_state->root_layer, &milton_state->working_stroke);

    glBindFramebuffer(GL_FRAMEBUFFER, render_data->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, export_texture, 0);

    gpu_render_canvas(render_data, 0, 0, buf_w, buf_h);

    // Resolve AA
    resolve_SSAA(render_data, export_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, export_fbo);

    // Read onto buffer
    glReadPixels(0,0,
                 buf_w/SSAA_FACTOR, buf_h/SSAA_FACTOR,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 (GLvoid*)data);

    {  // Flip texture
        u32* pixels = (u32*)data;
        for (i64 j=0;j < (buf_h/SSAA_FACTOR) / 2; ++j)
        {
            for (i64 i=0; i<buf_w/SSAA_FACTOR; ++i)
            {
                i64 idx_up = j*(buf_w/SSAA_FACTOR) + i;
                i64 j_down = buf_h/SSAA_FACTOR-1 - j;
                i64 idx_down = j_down*(buf_w/SSAA_FACTOR) + i;
                // Swap
                u32 pixel = pixels[idx_down];
                pixels[idx_down] = pixels[idx_up];
                pixels[idx_up] = pixel;
            }
        }
    }

    milton_save_buffer_to_file(TO_PATH_STR("test.png"), data, buf_w/SSAA_FACTOR,buf_h/SSAA_FACTOR);

    // Cleanup.
    mlt_free(data);

    render_data->fbo = saved_fbo;
    *milton_state->view = saved_view;
    render_data->width = saved_width;
    render_data->height = saved_height;

    glDeleteFramebuffers(1, &export_fbo);
    glDeleteTextures(1, &export_texture);
    glDeleteTextures(1, &buffer_texture);
    glBindFramebuffer(GL_FRAMEBUFFER, render_data->fbo);

    gpu_resize(render_data, view);
    gpu_set_canvas(render_data, view);
}

