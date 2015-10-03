//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


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
b32* filter_strokes_to_rect(Arena* arena,
                                 StrokeCord* strokes,
                                 const Rect rect)
{
    i32 num_strokes = strokes->count;
    b32* mask_array = arena_alloc_array(arena, num_strokes, b32);
    if (!mask_array) {
        return NULL;
    }

    for (i32 stroke_i = 0; stroke_i < num_strokes; ++stroke_i) {
        Stroke* stroke = StrokeCord_get(strokes, stroke_i);
        Rect stroke_rect = rect_enlarge(rect, stroke->brush.radius);
        VALIDATE_RECT(stroke_rect);
        if (stroke->num_points == 1) {
            if (is_inside_rect_scalar(stroke_rect, stroke->points_x[0], stroke->points_y[0])) {
                mask_array[stroke_i] = true;
            }
        } else {
            for (i32 point_i = 0; point_i < stroke->num_points - 1; ++point_i) {
                v2i a = (v2i){stroke->points_x[point_i], stroke->points_y[point_i]};
                v2i b = (v2i){stroke->points_x[point_i + 1], stroke->points_y[point_i + 1]};

                b32 inside = !((a.x > stroke_rect.right && b.x >  stroke_rect.right) ||
                               (a.x < stroke_rect.left && b.x <   stroke_rect.left) ||
                               (a.y < stroke_rect.top && b.y <    stroke_rect.top) ||
                               (a.y > stroke_rect.bottom && b.y > stroke_rect.bottom));

                if (inside) {
                    mask_array[stroke_i] = true;
                    break;
                }
            }
        }
    }
    return mask_array;
}

// Does point p0 with radius r0 contain point p1 with radius r1?
b32 stroke_point_contains_point(v2i p0, i32 r0,
                                v2i p1, i32 r1)
{
    v2i d = sub_v2i(p1, p0);
    // using manhattan distance, less chance of overflow. Still works well enough for this case.
    i32 m = abs(d.x) + abs(d.y) + r1;
    //i32 m = magnitude_i(d) + r1;
    assert(m >= 0);
    b32 contained = (m < r0);
    return contained;
}

Rect bounding_box_for_stroke(Stroke* stroke)
{
    Rect bb = bounding_rect_for_points_scalar(stroke->points_x, stroke->points_y, stroke->num_points);
    bb = rect_enlarge(bb, stroke->brush.radius);
    return bb;
}

Rect bounding_box_for_last_n_points(Stroke* stroke, i32 last_n)
{
    i32 forward = max(stroke->num_points - last_n, 0);
    i32 num_points = min(last_n, stroke->num_points);
    Rect bb = bounding_rect_for_points_scalar(stroke->points_x + forward,
                                       stroke->points_y + forward,
                                       num_points);
    bb = rect_enlarge(bb, stroke->brush.radius);
    return bb;
}

Rect canvas_rect_to_raster_rect(CanvasView* view, Rect canvas_rect)
{
    Rect raster_rect;
    raster_rect.bot_right = canvas_to_raster(view, canvas_rect.bot_right);
    raster_rect.top_left = canvas_to_raster(view, canvas_rect.top_left);
    return raster_rect;
}
