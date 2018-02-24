// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
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

    i32     flags;  // LayerFlags[

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

// IMPORTANT: CanvasView needs to be a flat structure.
//            Changing it means changing the file format,
//            because the whole struct is saved to the mlt file
struct CanvasView
{
    v2i screen_size;            // Size in pixels
    i64 scale;                  // Zoom
    v2i zoom_center;            // In pixels
    v2l pan_center;             // In canvas scale
    v3f background_color;
    i32 working_layer_id;
    i32 num_layers;
};

// Used to load older MLT files.
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


b32     is_eraser(v4f color);


v2l     canvas_to_raster (CanvasView* view, v2l canvas_point);
v2l     raster_to_canvas (CanvasView* view, v2l raster_point);

b32     stroke_point_contains_point (v2l p0, i64 r0, v2l p1, i64 r1);  // Does point p0 with radius r0 contain point p1 with radius r1?
Rect    bounding_box_for_stroke (Stroke* stroke);
Rect    bounding_box_for_last_n_points (Stroke* stroke, i32 last_n);
Rect    canvas_rect_to_raster_rect (CanvasView* view, Rect canvas_rect);

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
