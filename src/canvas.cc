// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license
//

v4f k_eraser_color = {23,34,45,56};


v2i
canvas_to_raster(CanvasView* view, v2i canvas_point)
{
    v2i raster_point = {
        ((view->pan_vector.x + canvas_point.x) / view->scale) + view->zoom_center.x,
        ((view->pan_vector.y + canvas_point.y) / view->scale) + view->zoom_center.y,
    };
    return raster_point;
}

v2i
raster_to_canvas(CanvasView* view, v2i raster_point)
{
    v2i canvas_point = {
        ((raster_point.x - view->zoom_center.x) * view->scale) - view->pan_vector.x,
        ((raster_point.y - view->zoom_center.y) * view->scale) - view->pan_vector.y,
    };

    return canvas_point;
}

// Does point p0 with radius r0 contain point p1 with radius r1?
b32
stroke_point_contains_point(v2i p0, i32 r0, v2i p1, i32 r1)
{
    v2i d = sub2i(p1, p0);
    // using manhattan distance, less chance of overflow. Still works well enough for this case.
    u32 m = (u32)abs(d.x) + abs(d.y) + r1;
    //i32 m = magnitude_i(d) + r1;
    b32 contained = false;
    if ( r0 >= 0 ) {
        contained = (m < (u32)r0);
    } else {
        contained = true;
    }
    return contained;
}

b32
is_eraser(v4f color)
{
    b32 result = equ4f(color, k_eraser_color);
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
layer_get_topmost(Layer* root)
{
    Layer* layer = root;
    while ( layer->next ) {
        layer = layer->next;
    }
    return layer;
}
Layer*
layer_get_by_id(Layer* root_layer, i32 id)
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
