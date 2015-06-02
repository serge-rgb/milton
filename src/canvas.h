
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
} Stroke;

typedef struct CanvasView_s
{
    v2i     screen_size;    // Size in pixels
    int32   scale;          // Zoom
    v2i     screen_center;  // In pixels
    v2i     pan_vector;     // In canvas scale
#if 0
    int     rotation;       // Rotation in radians
    float   cos_sin_table[360][2];  // First cosine, then sine.
#endif
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

inline v2i canvas_to_raster(CanvasView* view, v2i canvas_point)
{
#if 0
    v2i raster_point = canvas_point;
    raster_point = add_v2i       (raster_point, view->pan_vector);
    raster_point = invscale_v2i  (raster_point, view->scale);
    raster_point = add_v2i       (raster_point, view->screen_center);
    return raster_point;
#else
    v2i raster_point =
    {
        ((view->pan_vector.x + canvas_point.x) / view->scale) + view->screen_center.x,
        ((view->pan_vector.y + canvas_point.y) / view->scale) + view->screen_center.y,
    };
    return raster_point;
#endif
}

inline v2i raster_to_canvas(CanvasView* view, v2i raster_point)
{
#if 0
    v2i canvas_point = raster_point;
    canvas_point = sub_v2i       (canvas_point, view->screen_center);
    canvas_point = scale_v2i     (canvas_point, view->scale);
    canvas_point = sub_v2i       (canvas_point, view->pan_vector);
    return canvas_point;
#else
    v2i canvas_point =
    {
        ((raster_point.x - view->screen_center.x) * view->scale) - view->pan_vector.x,
        ((raster_point.y - view->screen_center.y) * view->scale) - view->pan_vector.y,
    };
    return canvas_point;
#endif
}

