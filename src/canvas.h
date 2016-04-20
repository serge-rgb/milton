// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#pragma once


#include "common.h"
#include "memory.h"
#include "milton_configuration.h"
#include "utils.h"

typedef struct Brush
{
    i32 radius;  // This should be replaced by a BrushType and some union containing brush info.
    v4f color;
    f32 alpha;
} Brush;

typedef struct Stroke
{
    Brush   brush;
    v2i*    points;
    f32*    pressures;
    i32     num_points;
    i32     layer_id;
    b32     visibility[MAX_NUM_WORKERS];
} Stroke;

// IMPORTANT: CanvasView needs to be a flat structure.
typedef struct CanvasView
{
    v2i screen_size;            // Size in pixels
    i32 scale;                  // Zoom
    v2i screen_center;          // In pixels
    v2i pan_vector;             // In canvas scale
    i32 downsampling_factor;
    i32 canvas_radius_limit;
    v3f background_color;
    i32 working_layer_id;
    i32 num_layers;
} CanvasView;

enum LayerFlags
{
    LayerFlags_VISIBLE = (1<<0),
};

typedef struct Layer
{
    i32 id;

    Stroke* strokes;  // stretchy
    char*   name;

    i32     flags;

    struct Layer* prev;
    struct Layer* next;
} Layer;

v2i canvas_to_raster(CanvasView* view, v2i canvas_point);

v2i raster_to_canvas(CanvasView* view, v2i raster_point);

// Thread-safe
// Returns an array of `num_strokes` b32's, masking strokes to the rect.
b32* create_stroke_masks(Layer* root_layer, Rect rect);

// Does point p0 with radius r0 contain point p1 with radius r1?
b32 stroke_point_contains_point(v2i p0, i32 r0, v2i p1, i32 r1);

Rect bounding_box_for_stroke(Stroke* stroke);

Rect bounding_box_for_last_n_points(Stroke* stroke, i32 last_n);

Rect canvas_rect_to_raster_rect(CanvasView* view, Rect canvas_rect);

// ---- Layer functions.

Layer* layer_get_topmost(Layer* root);

// Get the topmost stroke for current layer.
Stroke layer_get_top_stroke(Layer* layer);

Layer* layer_get_by_id(Layer* root_layer, i32 id);

void layer_toggle_visibility(Layer* layer);

Stroke* layer_push_stroke(Layer* layer, Stroke stroke);

i32 number_of_layers(Layer* root);


void stroke_free(Stroke* stroke);
