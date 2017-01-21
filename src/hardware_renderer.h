// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once


struct LayerEffect;

// Draw data for single stroke
struct RenderElement
{
    GLuint  vbo_stroke;
    GLuint  vbo_pointa;
    GLuint  vbo_pointb;
    GLuint  indices;

    i64     count;
    union {
        struct {  // For when element is a stroke.
            v4f     color;
            i32     radius;
        };
        struct {  // For when element is layer.
            f32          layer_alpha;
            LayerEffect* effects;
        };
    };

    int     flags;  // RenderElementFlags enum;
};

enum RenderDataFlags
{
    RenderDataFlags_NONE = 0,

    RenderDataFlags_GUI_VISIBLE        = 1<<0,
    RenderDataFlags_EXPORTING          = 1<<1,
};

struct Arena;
struct RenderData;
struct ColorPicker;
struct RenderData;
struct CanvasView;
struct Exporter;
struct Stroke;
struct Layer;
struct MiltonState;

RenderData* gpu_allocate_render_data(Arena* arena);

b32 gpu_init(RenderData* render_data, CanvasView* view, ColorPicker* picker, i32 render_data_flags);


enum BrushOutlineEnum
{
    BrushOutline_NO_FILL = 1<<0,
    BrushOutline_FILL    = 1<<1,
};
void gpu_update_brush_outline(RenderData* render_data, i32 cx, i32 cy, i32 radius,
                              BrushOutlineEnum outline_enum = BrushOutline_NO_FILL,
                              v4f color = {});




// Send milton data to OpenGL
void gpu_resize(RenderData* render_data, CanvasView* view);
void gpu_update_picker(RenderData* render_data, ColorPicker* picker);
void gpu_update_scale(RenderData* render_data, i32 scale);
void gpu_update_export_rect(RenderData* render_data, Exporter* exporter);
void gpu_update_background(RenderData* render_data, v3f background_color);
void gpu_update_canvas(RenderData* render_data, CanvasView* view);

void gpu_get_viewport_limits(RenderData* render_data, float* out_viewport_limits);
i32  gpu_get_num_clipped_strokes(Layer* root_layer);



// TODO: Measure memory consumption of glBufferData and their ilk
enum CookStrokeOpt
{
    CookStroke_NEW                   = 0,
    CookStroke_UPDATE_WORKING_STROKE = 1,
};
void gpu_cook_stroke(Arena* arena, RenderData* render_data, Stroke* stroke,
                     CookStrokeOpt cook_option = CookStroke_NEW);

void gpu_free_strokes(MiltonState*);

// Creates OpenGL objects for strokes that are in view but are not loaded on the GPU. Deletes
// content for strokes that are far away.
enum ClipFlags
{
    ClipFlags_UPDATE_GPU_DATA   = 1<<0,  // Free all strokes that are far away.
    ClipFlags_JUST_CLIP         = 1<<1,
};
void gpu_clip_strokes_and_update(Arena* arena,
                                 RenderData* render_data,
                                 CanvasView* view,
                                 Layer* root_layer, Stroke* working_stroke,
                                 i32 x, i32 y, i32 w, i32 h, ClipFlags flags = ClipFlags_JUST_CLIP);

void gpu_reset_render_flags(RenderData* render_data, int flags);


void gpu_render(RenderData* render_data,  i32 view_x, i32 view_y, i32 view_width, i32 view_height);
void gpu_render_to_buffer(MiltonState* milton_state, u8* buffer, i32 scale, i32 x, i32 y, i32 w, i32 h, f32 background_alpha);

void gpu_release_data(RenderData* render_data);

