
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

    v2i     canvas_reference;

    int32   num_points;
} Stroke;

typedef struct CanvasView_s
{
    v2i     canvas_tile_focus;      // Coordinates are relative to this.
    v2i     screen_size;            // Size in pixels
    int32   scale;                  // Zoom
    v2i     screen_center;          // In pixels
    v2i     pan_vector;             // In canvas scale
    int32   downsampling_factor;
    int32   canvas_tile_radius;
} CanvasView;


// TODO: rotation feature?
// Rotation changes a couple of things:
//  - Anti-aliasing has to take it into account.
//  - Cosines and Sines are slow!
// It's one of those things where it is not clear if the cost outweighs the
// benefit.  In this case, the cost is a lot of rendering performance, because
// of the extra step in the canvas conversion.
//
// This feature is fundamentally different to rotation on a raster-based paint
// package.
#if 0
inline v2i rotate_to_view(v2i p, CanvasView* view)
{
    int d = view->rotation;
    float c = view->cos_sin_table[d][0];
    float s = view->cos_sin_table[d][1];
    float x = (float)p.x;
    float y = (float)p.y;
    v2i r =
    {
        (int32)(x * c - y * s),
        (int32)(x * s + y * c),
    };
    return r;
}
#endif

inline v2i canvas_to_raster(CanvasView* view, v2i canvas_reference, v2i canvas_point)
{
    int32 tile_diff_x = view->canvas_tile_focus.x - canvas_reference.x;
    int32 tile_diff_y = view->canvas_tile_focus.y - canvas_reference.y;

    int32 diff_x = tile_diff_x * view->canvas_tile_radius;
    int32 diff_y = tile_diff_y * view->canvas_tile_radius;

    v2i raster_point =
    {
        ((view->pan_vector.x + canvas_point.x + diff_x) / view->scale) + view->screen_center.x,
        ((view->pan_vector.y + canvas_point.y + diff_y) / view->scale) + view->screen_center.y,
    };
    return raster_point;
}

inline v2i raster_to_canvas(CanvasView* view, v2i* canvas_reference, v2i raster_point)
{
    int32 diff_x = 0;
    int32 diff_y = 0;
    if (canvas_reference)
    {
        int32 tile_diff_x = view->canvas_tile_focus.x - canvas_reference->x;
        int32 tile_diff_y = view->canvas_tile_focus.y - canvas_reference->y;

        canvas_reference->x += tile_diff_x;
        canvas_reference->y += tile_diff_y;
        diff_x = canvas_reference->x * view->canvas_tile_radius;
        diff_y = canvas_reference->y * view->canvas_tile_radius;
    }

    v2i canvas_point =
    {
        ((raster_point.x - view->screen_center.x) * view->scale) - view->pan_vector.x - diff_x,
        ((raster_point.y - view->screen_center.y) * view->scale) - view->pan_vector.y - diff_y,
    };

    return canvas_point;
}

// Returns an array of `num_strokes` bool32's, masking strokes to the rect.
static bool32* filter_strokes_to_rect(Arena* arena,
                                      Stroke* strokes,
                                      int32 num_strokes,
                                      Rect rect)
{
    bool32* mask_array = arena_alloc_array(arena, num_strokes, bool32);
    for (int32 stroke_i = 0; stroke_i < num_strokes; ++stroke_i)
    {
        Stroke* stroke = &strokes[stroke_i];
        Rect stroke_rect = rect_enlarge(rect, stroke->brush.radius);
        if (stroke->num_points == 1)
        {
            if (is_inside_rect(stroke_rect, stroke->points[0]))
            {
                mask_array[stroke_i] = true;
            }
        }
        else
        {
            for (int32 point_i = 0; point_i < stroke->num_points - 1; ++point_i)
            {
                v2i a = stroke->points[point_i];
                v2i b = stroke->points[point_i + 1];

                bool32 inside = !((a.x > stroke_rect.right && b.x >  stroke_rect.right) ||
                                  (a.x < stroke_rect.left && b.x <   stroke_rect.left) ||
                                  (a.y < stroke_rect.top && b.y <    stroke_rect.top) ||
                                  (a.y > stroke_rect.bottom && b.y > stroke_rect.bottom));

                if (inside)
                {
                    mask_array[stroke_i] = true;
                    break;
                }
            }
        }
    }

    return mask_array;
}
