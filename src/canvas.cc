// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "canvas.h"
#include "utils.h"

v2l
canvas_to_raster_with_scale(CanvasView* view, v2l canvas_point, i64 scale)
{

    f32 x = (canvas_point.x - view->pan_center.x);
    f32 y = (canvas_point.y - view->pan_center.y);


    f32 cos_angle = cosf(-view->angle);
    f32 sin_angle = sinf(-view->angle);

    v2l raster_point = {
        i64(x * cos_angle - y * sin_angle) / scale + view->zoom_center.x,
        i64(y * cos_angle + x * sin_angle) / scale + view->zoom_center.y,
    };
    return raster_point;
}

v2l
raster_to_canvas_with_scale(CanvasView* view, v2l raster_point, i64 scale)
{
    f32 cos_angle = cosf(view->angle);
    f32 sin_angle = sinf(view->angle);

    // i64 x = raster_point.x * cos_angle - raster_point.y * sin_angle;
    // i64 y = raster_point.y * cos_angle + raster_point.x * sin_angle;

    i64 x = (raster_point.x - view->zoom_center.x);
    i64 y = (raster_point.y - view->zoom_center.y);

    v2l canvas_point = {
        i64(x * cos_angle - y * sin_angle) * scale + view->pan_center.x,
        i64(y * cos_angle + x * sin_angle) * scale + view->pan_center.y,
    };

    return canvas_point;
}

v2l
raster_to_canvas(CanvasView* view, v2l raster_point)
{
    return raster_to_canvas_with_scale(view, raster_point, view->scale);
}

v2l
canvas_to_raster(CanvasView* view, v2l canvas_point)
{
    return canvas_to_raster_with_scale(view, canvas_point, view->scale);
}

Rect
raster_to_canvas_bounding_rect(CanvasView* view, i32 x, i32 y, i32 w, i32 h, i64 scale)
{
    Rect result = rect_without_size();

    v2l corners[4] = {
        v2l{ x, y },
        v2l{ x+w, y },
        v2l{ x+w, y+h },
        v2l{ x, y+h },
    };

    for (int i = 0; i < 4; ++i) {
        v2l p = raster_to_canvas_with_scale(view, corners[i], scale);
        if (p.x < result.left) {
            result.left = p.x;
        }
        if (p.x > result.right) {
            result.right = p.x;
        }
        if (p.y < result.top) {
            result.top = p.y;
        }
        if (p.y > result.bottom) {
            result.bottom = p.y;
        }
    }

    return result;
}

Rect
canvas_to_raster_bounding_rect(CanvasView* view, Rect rect)
{
    Rect result = rect_without_size();

    v2l corners[4] = {
        v2l{ rect.left, rect.top },
        v2l{ rect.right, rect.top },
        v2l{ rect.right, rect.bottom },
        v2l{ rect.left, rect.bottom },
    };

    for (int i = 0; i < 4; ++i) {
        v2l p = canvas_to_raster(view, corners[i]);
        if (p.x < result.left) {
            result.left = p.x;
        }
        if (p.x > result.right) {
            result.right = p.x;
        }
        if (p.y < result.top) {
            result.top = p.y;
        }
        if (p.y > result.bottom) {
            result.bottom = p.y;
        }
    }

    return result;
}

void
reset_transform_at_origin(v2l* pan_center, i64* scale, f32* angle)
{
    *pan_center = {};
    *scale = 1024;
    *angle = 0.0f;
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


namespace layer
{
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
