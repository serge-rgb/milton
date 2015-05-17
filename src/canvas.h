
typedef struct Brush_s
{
    int32 radius;  // This should be replaced by a BrushType and some union containing brush info.
    v3f   color;
    float alpha;
} Brush;

#define LIMIT_STROKE_POINTS 1024
typedef struct Stroke_s
{
    Brush   brush;
    // TODO turn this into a deque
    v2i     points[LIMIT_STROKE_POINTS];
    v2i     clipped_points[2 * LIMIT_STROKE_POINTS];  // Clipped points are in the form [ab bc cd df]
                                                        // That's why we double the space for them.
    int32   num_points;
    int32   num_clipped_points;
    Rect    bounds;
} Stroke;


inline v2i canvas_to_raster(v2i screen_size, int32 view_scale, v2i canvas_point)
{
    v2i screen_center = invscale_v2i(screen_size, 2);
    v2i point = canvas_point;
    point = invscale_v2i(point, view_scale);
    point = add_v2i     ( point, screen_center );
    return point;
}

inline v2i raster_to_canvas(v2i screen_size, int32 view_scale, v2i raster_point)
{
    v2i screen_center = invscale_v2i(screen_size, 2);
    v2i canvas_point = raster_point;
    canvas_point = sub_v2i   ( canvas_point ,  screen_center );
    canvas_point = scale_v2i (canvas_point, view_scale);
    return canvas_point;
}

// Very much the opposite of coding in a functional-programming style.  Every
// stroke has a duplicate buffer where it stores only the points that may be
// drawn. If (TODO: s/if/when) we eventually do multithreading, we would need
// to allocate these buffers per-call to keep things data-parallel.
static void stroke_clip_to_rect(Stroke* stroke, Rect rect)
{
    stroke->num_clipped_points = 0;
    if (stroke->num_points == 1)
    {
        if (is_inside_rect(rect, stroke->points[0]))
        {
            stroke->clipped_points[stroke->num_clipped_points++] = stroke->points[0];
        }
    }
    else
    {
        for (int32 point_i = 0; point_i < stroke->num_points - 1; ++point_i)
        {
            v2i a = stroke->points[point_i];
            v2i b = stroke->points[point_i + 1];

            // Very conservative...
            bool32 inside =
                !(
                        (a.x > rect.right && b.x > rect.right) ||
                        (a.x < rect.left && b.x < rect.left) ||
                        (a.y < rect.top && b.y < rect.top) ||
                        (a.y > rect.bottom && b.y > rect.bottom)
                 );

            // We can add the segment
            if (inside)
            {
                stroke->clipped_points[stroke->num_clipped_points++] = a;
                stroke->clipped_points[stroke->num_clipped_points++] = b;
            }
        }
    }
}
