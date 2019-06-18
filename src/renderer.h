// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "common.h"
#include "system_includes.h"
#include "vector.h"

struct LayerEffect;

// Draw data for single stroke
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
        };
        struct {  // For when element is layer.
            f32          layer_alpha;
            LayerEffect* effects;
        };
    };

    int     flags;  // RenderElementFlags enum;
};

enum RenderBackendFlags
{
    RenderBackendFlags_NONE = 0,

    RenderBackendFlags_GUI_VISIBLE        = 1<<0,
    RenderBackendFlags_EXPORTING          = 1<<1,
    RenderBackendFlags_WITH_BLUR          = 1<<2,
};

struct Arena;
struct RenderBackend;
struct ColorPicker;
struct RenderBackend;
struct CanvasView;
struct Exporter;
struct Stroke;
struct Layer;
struct Milton;
struct CanvasState;

RenderBackend* gpu_allocate_render_backend(Arena* arena);

b32 gpu_init(RenderBackend* renderer, CanvasView* view, ColorPicker* picker);


// Immediate-mode functions

void imm_begin_frame(RenderBackend* renderer);
void imm_rect(RenderBackend* renderer, float left, float right, float top, float bottom, float line_width);
void imm_polygon(RenderBackend* renderer, v2f* points, i64 num_points, float line_width);

// End of immediate-mode functions

enum BrushOutlineEnum
{
    BrushOutline_NO_FILL = 1<<0,
    BrushOutline_FILL    = 1<<1,
};
void gpu_update_brush_outline(RenderBackend* renderer, i32 cx, i32 cy, i32 radius,
                              BrushOutlineEnum outline_enum = BrushOutline_NO_FILL,
                              v4f color = {});




void gpu_resize(RenderBackend* renderer, CanvasView* view);
void gpu_update_picker(RenderBackend* renderer, ColorPicker* picker);
void gpu_update_scale(RenderBackend* renderer, i32 scale);
void gpu_update_background(RenderBackend* renderer, v3f background_color);
void gpu_update_canvas(RenderBackend* renderer, CanvasState* canvas, CanvasView* view);

void gpu_get_viewport_limits(RenderBackend* renderer, float* out_viewport_limits);
i32  gpu_get_num_clipped_strokes(Layer* root_layer);


enum CookStrokeOpt
{
    CookStroke_NEW                   = 0,
    CookStroke_UPDATE_WORKING_STROKE = 1,
};
void gpu_cook_stroke(Arena* arena, RenderBackend* renderer, Stroke* stroke,
                     CookStrokeOpt cook_option = CookStroke_NEW);

void gpu_free_strokes(RenderBackend* renderer, CanvasState* canvas);


// Creates OpenGL objects for strokes that are in view but are not loaded on the GPU. Deletes
// content for strokes that are far away.
enum ClipFlags
{
    ClipFlags_UPDATE_GPU_DATA   = 1<<0,  // Free all strokes that are far away.
    ClipFlags_JUST_CLIP         = 1<<1,
};
void gpu_clip_strokes_and_update(Arena* arena,
                                 RenderBackend* renderer,
                                 CanvasView* view, i64 render_scale,
                                 Layer* root_layer, Stroke* working_stroke,
                                 i32 x, i32 y, i32 w, i32 h, ClipFlags flags = ClipFlags_JUST_CLIP);

void gpu_reset_render_flags(RenderBackend* renderer, int flags);

void gpu_render(RenderBackend* renderer,  i32 view_x, i32 view_y, i32 view_width, i32 view_height);
void gpu_render_to_buffer(Milton* milton, u8* buffer, i32 scale, i32 x, i32 y, i32 w, i32 h, f32 background_alpha);

void gpu_release_data(RenderBackend* renderer);

