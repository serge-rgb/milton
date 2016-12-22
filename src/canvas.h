// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once

#define MAX_LAYER_NAME_LEN          64

struct Layer
{
    i32 id;

    StrokeList strokes;
    char    name[MAX_LAYER_NAME_LEN];

    i32     flags;

    Layer* prev;
    Layer* next;
};


// IMPORTANT: CanvasView needs to be a flat structure.
//            Changing it means changing the file format,
//            because the whole struct is saved to the mlt file
struct CanvasView
{
    v2i screen_size;            // Size in pixels
    i32 scale;                  // Zoom
    v2i zoom_center;            // In pixels
    v2i pan_vector;             // In canvas scale
    i32 downsampling_factor;
    i32 canvas_radius_limit;
    v3f background_color;
    i32 working_layer_id;
    i32 num_layers;
};

enum LayerFlags
{
    LayerFlags_VISIBLE = (1<<0),
};
b32 is_eraser(Brush* brush);

v2i canvas_to_raster(CanvasView* view, v2i canvas_point);

v2i raster_to_canvas(CanvasView* view, v2i raster_point);

// Thread-safe
// Returns an array of `num_strokes` b32's, masking strokes to the rect.
b32*    create_stroke_masks(Layer* root_layer, Rect rect);

// Does point p0 with radius r0 contain point p1 with radius r1?
b32     stroke_point_contains_point(v2i p0, i32 r0, v2i p1, i32 r1);

Rect    bounding_box_for_stroke(Stroke* stroke);

Rect    bounding_box_for_last_n_points(Stroke* stroke, i32 last_n);

Rect    canvas_rect_to_raster_rect(CanvasView* view, Rect canvas_rect);

// ---- Layer functions.

Layer*  layer_get_topmost(Layer* root);

// Get the topmost stroke for current layer.
Stroke  layer_get_top_stroke(Layer* layer);

Layer*  layer_get_by_id(Layer* root_layer, i32 id);

void    layer_toggle_visibility(Layer* layer);

Stroke* layer_push_stroke(Layer* layer, Stroke stroke);

typedef struct MiltonState MiltonState;
void    layer_new(MiltonState* milton_state);

i32     number_of_layers(Layer* root);

void    free_layers(Layer* root);

i64     count_strokes(Layer* root);
i64     count_clipped_strokes(Layer* root, i32 num_workers);

void    stroke_free(Stroke* stroke);

