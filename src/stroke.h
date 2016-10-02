struct Brush
{
    i32 radius;  // This should be replaced by a BrushType and some union containing brush info.
    v4f color;
    f32 alpha;
};

struct Stroke
{
    i32             id;

    Brush           brush;
    v2i*            points;
    f32*            pressures;
    i32             num_points;
    i32             layer_id;
#if SOFTWARE_RENDERER_COMPILED
    b32             visibility[MAX_NUM_WORKERS];
#endif
    Rect            bounding_rect;
    RenderElement   render_element;
};