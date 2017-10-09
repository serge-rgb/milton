// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "canvas.h"
#include "utils.h"

v4f k_eraser_color = {23,34,45,56};

v2l
canvas_to_raster(CanvasView* view, v2l canvas_point)
{
    v2l raster_point = {
        ((canvas_point.x - view->pan_center.x) / view->scale) + view->zoom_center.x,
        ((canvas_point.y - view->pan_center.y) / view->scale) + view->zoom_center.y,
    };
    return raster_point;
}

v2l
raster_to_canvas(CanvasView* view, v2l raster_point)
{
    v2l canvas_point = {
        ((raster_point.x - view->zoom_center.x) * view->scale) + view->pan_center.x,
        ((raster_point.y - view->zoom_center.y) * view->scale) + view->pan_center.y,
    };

    return canvas_point;
}

// Does point p0 with radius r0 contain point p1 with radius r1?
b32
stroke_point_contains_point(v2l p0, i64 r0, v2l p1, i64 r1)
{
    v2l d = p1 - p0;
    // using manhattan distance, less chance of overflow. Still works well enough for this case.
    u64 m = (u64)MLT_ABS(d.x) + MLT_ABS(d.y) + r1;
    //i32 m = magnitude_i(d) + r1;
    b32 contained = false;
    if ( r0 >= 0 ) {
        contained = (m < (u64)r0);
    } else {
        contained = true;
    }
    return contained;
}

b32
is_eraser(v4f color)
{
    b32 result = color == k_eraser_color;
    return result;
}

Rect
bounding_box_for_stroke(Stroke* stroke)
{
    Rect bb = bounding_rect_for_points(stroke->points, stroke->num_points);
    Rect bb_enlarged = rect_enlarge(bb, stroke->brush.radius);
    return bb_enlarged;
}

Rect
bounding_box_for_last_n_points(Stroke* stroke, i32 last_n)
{
    i32 forward = max(stroke->num_points - last_n, 0);
    i32 num_points = min(last_n, stroke->num_points);
    Rect bb = bounding_rect_for_points(stroke->points + forward, num_points);
    Rect bb_enlarged = rect_enlarge(bb, stroke->brush.radius);
    return bb_enlarged;
}

Rect
canvas_rect_to_raster_rect(CanvasView* view, Rect canvas_rect)
{
    Rect raster_rect;
    raster_rect.bot_right = canvas_to_raster(view, canvas_rect.bot_right);
    raster_rect.top_left = canvas_to_raster(view, canvas_rect.top_left);
    return raster_rect;
}


namespace layer {

i64
count_strokes(Layer* root)
{
    i64 count = 0;
    for ( Layer *layer = root;
          layer != NULL;
          layer = layer->next ) {
        count += layer->strokes.count;
    }
    return count;
}

#if SOFTWARE_RENDERER_COMPILED
i64
count_clipped_strokes(Layer* root, i32 num_workers)
{
    i64 count = 0;
    for ( Layer *layer = root;
          layer != NULL;
          layer = layer->next ) {
        u64 num_strokes = layer->strokes.count;
        for ( u64 i = 0; i < num_strokes; ++i ) {
            Stroke* s = layer->strokes.data + i;
            for ( i32 wi = 0; wi < num_workers; ++wi ) {
                if ( s->visibility[wi] ) {
                    ++count;
                    break;
                }
            }
        }
    }
    return count;
}
#endif

Layer*
get_topmost(Layer* root)
{
    Layer* layer = root;
    while ( layer->next ) {
        layer = layer->next;
    }
    return layer;
}
Layer*
get_by_id(Layer* root_layer, i32 id)
{
    Layer* l = NULL;
    for ( Layer* layer = root_layer; layer; layer = layer->next ) {
        if ( layer->id == id ) {
            l = layer;
        }
    }
    return l;
}

// Push stroke at the top of the current layer
Stroke*
layer_push_stroke(Layer* layer, Stroke stroke)
{
    push(&layer->strokes, stroke);
    return peek(&layer->strokes);
}

b32
layer_has_blur_effect(Layer* layer)
{
    b32 result = false;
    for ( LayerEffect* e = layer->effects; e != NULL; e = e->next ) {
        if ( e->enabled && e->type == LayerEffectType_BLUR ) {
            result = true;
            break;
        }
    }
    return result;
}


void
layer_toggle_visibility(Layer* layer)
{
    b32 visible = layer->flags & LayerFlags_VISIBLE;
    if ( visible ) {
        layer->flags &= ~LayerFlags_VISIBLE;
    } else {
        layer->flags |= LayerFlags_VISIBLE;
    }
}

i32
number_of_layers(Layer* layer)
{
    int n = 0;
    while ( layer ) {
        ++n;
        layer = layer->next;
    }
    return n;
}

}  // namespace layer
