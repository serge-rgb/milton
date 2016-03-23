// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.


#include "canvas.h"

#include "common.h"
#include "canvas.h"
#include "memory.h"
#include "platform.h"
#include "utils.h"


v2i canvas_to_raster(CanvasView* view, v2i canvas_point)
{
    v2i raster_point = {
        ((view->pan_vector.x + canvas_point.x) / view->scale) + view->screen_center.x,
        ((view->pan_vector.y + canvas_point.y) / view->scale) + view->screen_center.y,
    };
    return raster_point;
}

v2i raster_to_canvas(CanvasView* view, v2i raster_point)
{
    v2i canvas_point = {
        ((raster_point.x - view->screen_center.x) * view->scale) - view->pan_vector.x,
        ((raster_point.y - view->screen_center.y) * view->scale) - view->pan_vector.y,
    };

    return canvas_point;
}

// Returns an array of `num_strokes` b32's, masking strokes to the rect.
void filter_strokes_to_rect(Layer* layer, Rect rect)
{
    while ( layer ) {
        Stroke* strokes = layer->strokes;
        if ( layer->masks_count < sb_count(layer->strokes) ) {
            if ( layer->masks ) mlt_free(layer->masks);
            layer->masks_count = 2*sb_count(layer->strokes);
            layer->masks = mlt_calloc(layer->masks_count, sizeof(b32));
        }

        for (i32 stroke_i = 0; stroke_i < sb_count(layer->strokes); ++stroke_i) {
            Stroke* stroke = &strokes[stroke_i];
            Rect stroke_rect = rect_enlarge(rect, stroke->brush.radius);
            if ( rect_is_valid(stroke_rect) ) {
                if (stroke->num_points == 1) {
                    if ( is_inside_rect(stroke_rect, stroke->points[0]) ) {
                        layer->masks[stroke_i] = true;
                    }
                } else {
                    for (size_t point_i = 0; point_i < (size_t)stroke->num_points - 1; ++point_i) {
                        v2i a = stroke->points[point_i    ];
                        v2i b = stroke->points[point_i + 1];

                        b32 inside = !((a.x > stroke_rect.right && b.x >  stroke_rect.right) ||
                                       (a.x < stroke_rect.left && b.x <   stroke_rect.left) ||
                                       (a.y < stroke_rect.top && b.y <    stroke_rect.top) ||
                                       (a.y > stroke_rect.bottom && b.y > stroke_rect.bottom));

                        if (inside) {
                            layer->masks[stroke_i] = true;
                            break;
                        }
                    }
                }
            } else {
                milton_log("Discarging stroke %x because of invalid rect.\n", stroke);
            }
        }
        layer = layer->next;
    }
}

// Does point p0 with radius r0 contain point p1 with radius r1?
b32 stroke_point_contains_point(v2i p0, i32 r0, v2i p1, i32 r1)
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

Rect bounding_box_for_stroke(Stroke* stroke)
{
    Rect bb = bounding_rect_for_points(stroke->points, stroke->num_points);
    Rect bb_enlarged = rect_enlarge(bb, stroke->brush.radius);
    return bb_enlarged;
}

Rect bounding_box_for_last_n_points(Stroke* stroke, i32 last_n)
{
    i32 forward = max(stroke->num_points - last_n, 0);
    i32 num_points = min(last_n, stroke->num_points);
    Rect bb = bounding_rect_for_points(stroke->points + forward, num_points);
    Rect bb_enlarged = rect_enlarge(bb, stroke->brush.radius);
    return bb_enlarged;
}

Rect canvas_rect_to_raster_rect(CanvasView* view, Rect canvas_rect)
{
    Rect raster_rect;
    raster_rect.bot_right = canvas_to_raster(view, canvas_rect.bot_right);
    raster_rect.top_left = canvas_to_raster(view, canvas_rect.top_left);
    return raster_rect;
}

Layer* layer_get_topmost(Layer* root)
{
    Layer* layer = root;
    while ( layer->next ) {
        layer = layer->next;
    }
    return layer;
}

// Push stroke at the top of the current layer
Stroke* layer_push_stroke(Layer* layer, Stroke stroke)
{
    sb_push(layer->strokes, stroke);
    return &sb_peek(layer->strokes);
}

void layer_toggle_visibility(Layer* layer)
{
    b32 visible = layer->flags | LayerFlags_VISIBLE;
    if ( visible ) {
        layer->flags &= ~LayerFlags_VISIBLE;
    } else {
        layer->flags |= LayerFlags_VISIBLE;
    }
}

i32 number_of_layers(Layer* layer)
{
    int n = 0;
    while ( layer ) {
        ++n;
        layer = layer->next;
    }
    return n;
}
