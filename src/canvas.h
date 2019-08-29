// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#include "vector.h"
#include "StrokeList.h"

#define MAX_LAYER_NAME_LEN          64

struct LayerEffect
{
    i32 type;  // LayerEffectType enum
    b32 enabled;

    union {
        struct {  // Blur
            // In order for the blur to be resolution independent, we
            // save the zoom level at the time of creation and always
            // multiply the kernel size by original_scale / CURRENT_SCALE
            i32 original_scale;
            i32 kernel_size;
        } blur;
    };

    LayerEffect* next;
};

enum LayerFlags
{
    LayerFlags_VISIBLE = (1<<0),
};

struct Layer
{
    i32 id;

    StrokeList strokes;
    char    name[MAX_LAYER_NAME_LEN];

    i32     flags;  // LayerFlags

    float alpha;

    LayerEffect* effects;

    Layer* prev;
    Layer* next;
};

enum LayerEffectType
{
    LayerEffectType_BLUR,

    LayerEffectType_COUNT,
};

#pragma pack (push, 1)

// IMPORTANT: CanvasView needs to be a flat structure
//            because the whole struct is saved to the mlt file.
//            It should only grow down.
struct CanvasView
{
    u32 size;                   // Size of struct
    v2i screen_size;            // Size in pixels
    i64 scale;                  // Zoom
    v2i zoom_center;            // In pixels
    v2l pan_center;             // In canvas scale
    v3f background_color;
    i32 working_layer_id;
    f32 angle;                  // Rotation
};

// Used to load older MLT files.
struct CanvasViewPreV9
{
    v2i screen_size;            // Size in pixels
    i64 scale;                  // Zoom
    v2i zoom_center;            // In pixels
    v2l pan_center;             // In canvas scale
    v3f background_color;
    i32 working_layer_id;
    i32 num_layers;
    u8 padding[4];
};
struct CanvasViewPreV4
{
    v2i screen_size;            // Size in pixels
    i32 scale;                  // Zoom
    v2i zoom_center;            // In pixels
    v2i pan_center;             // In canvas scale
    i32 downsampling_factor;
    i32 canvas_radius_limit;
    v3f background_color;
    i32 working_layer_id;
    i32 num_layers;
};

#pragma pack(pop)


v2l     canvas_to_raster (CanvasView* view, v2l canvas_point);
v2l     raster_to_canvas (CanvasView* view, v2l raster_point);

b32     stroke_point_contains_point (v2l p0, i64 r0, v2l p1, i64 r1);  // Does point p0 with radius r0 contain point p1 with radius r1?
Rect    bounding_box_for_stroke (Stroke* stroke);
Rect    bounding_box_for_last_n_points (Stroke* stroke, i32 last_n);

Rect    raster_to_canvas_bounding_rect(CanvasView* view, i32 x, i32 y, i32 w, i32 h, i64 scale);
Rect    canvas_to_raster_bounding_rect(CanvasView* view, Rect rect);

void    reset_transform_at_origin(v2l* pan_center, i64* scale, f32* angle);

// ---- Layer functions.


namespace layer {
    Layer*  get_topmost (Layer* root);
    Layer*  get_by_id (Layer* root_layer, i32 id);
    void    layer_toggle_visibility (Layer* layer);
    b32     layer_has_blur_effect (Layer* layer);
    Stroke* layer_push_stroke (Layer* layer, Stroke stroke);
    i32     number_of_layers (Layer* root);
    void    free_layers (Layer* root);
    i64     count_strokes (Layer* root);
    i64     count_clipped_strokes (Layer* root, i32 num_workers);
}
