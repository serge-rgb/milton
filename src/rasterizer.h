// rasterizer.h
// (c) Copyright 2015 by Sergio Gonzalez


typedef struct ClippedStroke_s ClippedStroke;
struct ClippedStroke_s
{
    bool32  fills_block;
    Brush   brush;
    int32   num_points;
    v2i     canvas_reference;
    v2i*    points;
    ClippedStroke* next;
};

static ClippedStroke* stroke_clip_to_rect(Arena* render_arena, Stroke* stroke, Rect rect)
{
    ClippedStroke* clipped_stroke = arena_alloc_elem(render_arena, ClippedStroke);
    {
        clipped_stroke->brush = stroke->brush;
        clipped_stroke->num_points = 0;
        clipped_stroke->points = arena_alloc_array(render_arena, stroke->num_points * 2, v2i);
        clipped_stroke->canvas_reference = stroke->canvas_reference;
    }
    if (stroke->num_points == 1)
    {
        if (is_inside_rect(rect, stroke->points[0]))
        {
            clipped_stroke->points[clipped_stroke->num_points++] = stroke->points[0];
        }
    }
    else
    {
        int32 num_points = stroke->num_points;
        for (int32 point_i = 0; point_i < num_points - 1; ++point_i)
        {
            v2i a = stroke->points[point_i];
            v2i b = stroke->points[point_i + 1];

            // Very conservative...
            bool32 inside = !((a.x > rect.right && b.x > rect.right) ||
                              (a.x < rect.left && b.x < rect.left) ||
                              (a.y < rect.top && b.y < rect.top) ||
                              (a.y > rect.bottom && b.y > rect.bottom));

            // We can add the segment
            if (inside)
            {
                clipped_stroke->points[clipped_stroke->num_points++] = a;
                clipped_stroke->points[clipped_stroke->num_points++] = b;
            }
        }
    }
    return clipped_stroke;
}

inline v2i closest_point_in_segment(
        v2i a, v2i b, v2f ab, float ab_magnitude_squared, v2i canvas_point)
{
    v2i point;
    float mag_ab = sqrtf(ab_magnitude_squared);
    float d_x = ab.x / mag_ab;
    float d_y = ab.y / mag_ab;
    float ax_x = (float)(canvas_point.x - a.x);
    float ax_y = (float)(canvas_point.y - a.y);
    float disc = d_x * ax_x + d_y * ax_y;
    if (disc >= 0 && disc <= mag_ab)
    {
        point = (v2i)
        {
            (int32)(a.x + disc * d_x), (int32)(a.y + disc * d_y),
        };
    }
    else if (disc < 0)
    {
        point = a;
    }
    else
    {
        point = b;
    }
    return point;
}

// NOTE: takes clipped points.
inline bool32 is_rect_filled_by_stroke(
        Rect rect, v2i* points, int32 num_points, Brush brush, CanvasView* view)
{
    v2i rect_center =
    {
        (rect.left + rect.right) / 2,
        (rect.top + rect.bottom) / 2,
    };

    if (num_points >= 2)
    {
        for (int32 point_i = 0; point_i < num_points; point_i += 2)
        {
            v2i a = points[point_i];
            v2i b = points[point_i + 1];

            // Get closest point
            v2f ab = {(float)(b.x - a.x), (float)(b.y - a.y)};
            float mag_ab2 = ab.x * ab.x + ab.y * ab.y;
            v2i p  = closest_point_in_segment( a, b, ab, mag_ab2, rect_center);

            // Half width of a rectangle contained by brush at point p.
            int32 rad = (int32)(brush.radius * 0.707106781f);  // cos(pi/4)
            Rect bounded_rect;
            {
                bounded_rect.left   = p.x - rad;
                bounded_rect.right  = p.x + rad;
                bounded_rect.bottom = p.y + rad;
                bounded_rect.top    = p.y - rad;
            }

            if (is_rect_within_rect(rect, bounded_rect))
            {
                return true;
            }
        }
    }
    else if (num_points == 1)
    {
        v2i p  = points[0];

        // Half width of a rectangle contained by brush at point p.
        int32 rad = (int32)(brush.radius * 0.707106781f);  // cos(pi/4)
        Rect bounded_rect;
        {
            bounded_rect.left   = p.x - rad;
            bounded_rect.right  = p.x + rad;
            bounded_rect.bottom = p.y + rad;
            bounded_rect.top    = p.y - rad;
        }

        if (is_rect_within_rect(rect, bounded_rect))
        {
            return true;
        }
    }
    return false;
}

inline void render_canvas_in_block(Arena* render_arena,
                                   CanvasView* view,
                                   ColorManagement cm,
                                   Stroke* strokes,
                                   bool32* stroke_masks,
                                   int32   num_strokes,
                                   Stroke* working_stroke,
                                   uint32* pixels,
                                   Rect raster_limits)
{

    v2i canvas_reference = { 0 };

    Rect canvas_limits;
    {
        canvas_limits.top_left = raster_to_canvas(view,  &canvas_reference, raster_limits.top_left);
        v2i second_cr;
        canvas_limits.bot_right = raster_to_canvas(view, &second_cr, raster_limits.bot_right);
        assert (canvas_reference.x == second_cr.x &&
                canvas_reference.y == second_cr.y);
    }

    v2i diff =
    {
        (canvas_reference.x) * view->canvas_tile_radius,
        (canvas_reference.y) * view->canvas_tile_radius,
    };
#if 0
    if (canvas_limits.left    -diff.x < -view->canvas_tile_radius ||
        canvas_limits.right   -diff.x > view->canvas_tile_radius ||
        canvas_limits.top     -diff.y < -view->canvas_tile_radius ||
        canvas_limits.bottom  -diff.y > view->canvas_tile_radius
        )
#else
    if (canvas_limits.left   < -view->canvas_tile_radius ||
        canvas_limits.right  > view->canvas_tile_radius ||
        canvas_limits.top    < -view->canvas_tile_radius ||
        canvas_limits.bottom > view->canvas_tile_radius
        )
#endif
    {
        for (int j = raster_limits.top; j < raster_limits.bottom; j++)
        {
            for (int i = raster_limits.left; i < raster_limits.right; i++)
            {
                pixels[j * view->screen_size.w + i] = 0xffff00ff;
            }
        }
        return;
    }
    ClippedStroke* stroke_list = NULL;

    // Go backwards so that list is in the correct older->newer order.
    for (int stroke_i = num_strokes; stroke_i >= 0; --stroke_i)
    {
        if (stroke_i < num_strokes && !stroke_masks[stroke_i])
        {
            continue;
        }
        Stroke* stroke = NULL;
        if (stroke_i == num_strokes)
        {
            stroke = working_stroke;
        }
        else
        {
            stroke = &strokes[stroke_i];
        }
        assert(stroke);
        Rect enlarged_limits = rect_enlarge(canvas_limits, stroke->brush.radius);
        ClippedStroke* clipped_stroke = stroke_clip_to_rect(render_arena, stroke, enlarged_limits);
        if (clipped_stroke->num_points)
        {
            ClippedStroke* list_head = clipped_stroke;
            list_head->next = stroke_list;
            if (is_rect_filled_by_stroke(
                        canvas_limits,
                        clipped_stroke->points, clipped_stroke->num_points, clipped_stroke->brush,
                        view))
            {
                list_head->fills_block = true;
            }
            stroke_list = list_head;
        }
    }

    // Set our `stroke_list` to begin at the first opaque stroke that fills
    // this block.
    ClippedStroke* list_iter = stroke_list;
    while (list_iter)
    {
        if (list_iter->fills_block && list_iter->brush.alpha == 1.0f)
        {
            stroke_list = list_iter;
        }
        list_iter = list_iter->next;
    }

    int pixel_jump = view->downsampling_factor;  // Different names for the same thing.

    for (int j = raster_limits.top; j < raster_limits.bottom; j += pixel_jump)
    {
        for (int i = raster_limits.left; i < raster_limits.right; i += pixel_jump)
        {
            v2i raster_point = {i, j};
            v2i cr;
            v2i canvas_point = raster_to_canvas(view, &cr, raster_point);
            assert (cr.x == canvas_reference.x && cr.y == canvas_reference.y);
            /* canvas_point.x -= canvas_reference.x * view->canvas_tile_radius; */
            /* canvas_point.x -= canvas_reference.y * view->canvas_tile_radius; */

            // Clear color
            float dr = 1.0f;
            float dg = 1.0f;
            float db = 1.0f;
            float da = 0.0f;

            ClippedStroke* list_iter = stroke_list;
            while(list_iter)
            {
                ClippedStroke* clipped_stroke = list_iter;
                /* v2i stroke_canvas_reference = clipped_stroke->canvas_reference; */
                list_iter = list_iter->next;

                assert (clipped_stroke);

                // Fast path.
                if (clipped_stroke->fills_block)
                {
#if 0  // Visualize it with black
                    float sr = clipped_stroke->brush.color.r * 0;
                    float sg = clipped_stroke->brush.color.g * 0;
                    float sb = clipped_stroke->brush.color.b * 0;
#else
                    float sr = clipped_stroke->brush.color.r;
                    float sg = clipped_stroke->brush.color.g;
                    float sb = clipped_stroke->brush.color.b;
#endif
                    float sa = clipped_stroke->brush.alpha;

                    // Move to gamma space
                    float g_dr = dr * dr;
                    float g_dg = dg * dg;
                    float g_db = db * db;
                    sr = sr * sr;
                    sg = sg * sg;
                    sb = sb * sb;

                    g_dr = (1 - sa) * g_dr + sa * sr;
                    g_dg = (1 - sa) * g_dg + sa * sg;
                    g_db = (1 - sa) * g_db + sa * sb;
                    da = sa + da * (1 - sa);

                    // Back to linear space
                    dr = sqrtf(g_dr);
                    dg = sqrtf(g_dg);
                    db = sqrtf(g_db);

                }
                // Slow path. There are pixels not inside.
                else
                {
                    v2i* points = clipped_stroke->points;

                    v2i min_point = {0};
                    float min_dist = FLT_MAX;
                    float dx = 0;
                    float dy = 0;
                    //int64 radius_squared = stroke->brush.radius * stroke->brush.radius;
                    if (clipped_stroke->num_points == 1)
                    {
                        min_point = points[0];
                        dx = (float) (canvas_point.x - min_point.x);
                        dy = (float) (canvas_point.y - min_point.y);
                        min_dist = dx * dx + dy * dy;
                    }
                    else
                    {
                        // Find closest point.
                        for (int point_i = 0; point_i < clipped_stroke->num_points-1; point_i += 2)
                        {
                            v2i a = points[point_i];
                            v2i b = points[point_i + 1];
#if 0
                            a.x -= stroke_canvas_reference.x * view->canvas_tile_radius;
                            a.y -= stroke_canvas_reference.x * view->canvas_tile_radius;
                            b.x -= stroke_canvas_reference.x * view->canvas_tile_radius;
                            b.y -= stroke_canvas_reference.x * view->canvas_tile_radius;
#endif

                            v2f ab = {(float)(b.x - a.x), (float)(b.y - a.y)};
                            float mag_ab2 = ab.x * ab.x + ab.y * ab.y;
                            if (mag_ab2 > 0)
                            {
                                v2i point = closest_point_in_segment(a, b,
                                                                     ab, mag_ab2, canvas_point);

                                float test_dx = (float) (canvas_point.x - point.x);
                                float test_dy = (float) (canvas_point.y - point.y);
                                float dist = test_dx * test_dx + test_dy * test_dy;
                                if (dist < min_dist)
                                {
                                    min_dist = dist;
                                    min_point = point;
                                    dx = test_dx;
                                    dy = test_dy;
                                }
                            }
                        }
                    }

                    if (min_dist < FLT_MAX)
                    {
                        int samples = 0;
                        {
                            float u = 0.223607f * view->scale;  // sin(arctan(1/2)) / 2
                            float v = 0.670820f * view->scale;  // cos(arctan(1/2)) / 2 + u

                            float dists[4];
                            dists[0] = (dx - u) * (dx - u) + (dy - v) * (dy - v);
                            dists[1] = (dx - v) * (dx - v) + (dy + u) * (dy + u);
                            dists[2] = (dx + u) * (dx + u) + (dy + v) * (dy + v);
                            dists[3] = (dx + v) * (dx + v) + (dy + u) * (dy + u);
                            for (int i = 0; i < 4; ++i)
                            {
                                if (sqrtf(dists[i]) < clipped_stroke->brush.radius)
                                {
                                    ++samples;
                                }
                            }
                        }

                        // If the stroke contributes to the pixel, do compositing.
                        if (samples > 0)
                        {
                            // Do compositing
                            // ---------------

                            float coverage = (float)samples / 4.0f;

                            float sr = clipped_stroke->brush.color.r;
                            float sg = clipped_stroke->brush.color.g;
                            float sb = clipped_stroke->brush.color.b;
                            float sa = clipped_stroke->brush.alpha;

                            sa *= coverage;

                            // Move to gamma space
                            float g_dr = dr * dr;
                            float g_dg = dg * dg;
                            float g_db = db * db;
                            sr = sr * sr;
                            sg = sg * sg;
                            sb = sb * sb;

                            g_dr = (1 - sa) * g_dr + sa * sr;
                            g_dg = (1 - sa) * g_dg + sa * sg;
                            g_db = (1 - sa) * g_db + sa * sb;
                            da = sa + da * (1 - sa);

                            // Back to linear space
                            dr = sqrtf(g_dr);
                            dg = sqrtf(g_dg);
                            db = sqrtf(g_db);
                        }
                    }

                }
            }
            // From [0, 1] to [0, 255]
            v4f d = {
                dr, dg, db, da
            };
            uint32 pixel = color_v4f_to_u32(cm, d);

            // TODO: Bilinear sampling could be nice here
            for (int jj = j; jj < j + pixel_jump; ++jj)
            {
                for (int ii = i; ii < i + pixel_jump; ++ii)
                {
                    pixels[jj * view->screen_size.w + ii] = pixel;
                }
            }
        }
    }
}

static bool32 render_canvas(MiltonState* milton_state, uint32* raster_buffer, Rect raster_limits)
{
    v2i canvas_reference = { 0 };
    Rect* blocks = NULL;
    int32 num_blocks = rect_split(milton_state->transient_arena,
            raster_limits, milton_state->block_width, milton_state->block_width, &blocks);


    const int32 blocks_per_tile = milton_state->blocks_per_tile;

    // TODO: make this loop data parallel
    for (int i = 0; i < num_blocks; i += blocks_per_tile)
    {
        if (i >= num_blocks)
        {
            break;
        }

        Arena tile_arena = arena_push(milton_state->transient_arena,
                                      arena_available_space(milton_state->transient_arena));

        Rect raster_tile_rect = { 0 };
        Rect canvas_tile_rect = { 0 };

        // Clip and move to canvas space.
        // Derive tile_rect
        for (int block_i = 0; block_i < blocks_per_tile; ++block_i)
        {
            if (i + block_i >= num_blocks)
            {
                break;
            }
            blocks[i + block_i] = rect_clip_to_screen(blocks[i + block_i],
                                                      milton_state->view->screen_size);
            raster_tile_rect = rect_union(raster_tile_rect, blocks[i + block_i]);

            canvas_tile_rect.top_left =
                raster_to_canvas(milton_state->view, &canvas_reference, raster_tile_rect.top_left);
            canvas_tile_rect.bot_right =
                raster_to_canvas(milton_state->view, &canvas_reference, raster_tile_rect.bot_right);
        }

        // Filter strokes to this tile.
        bool32* stroke_masks = filter_strokes_to_rect(&tile_arena,
                                                      milton_state->strokes,
                                                      milton_state->num_strokes,
                                                      canvas_tile_rect);

        for (int block_i = 0; block_i < blocks_per_tile; ++block_i)
        {
            if (i + block_i >= num_blocks)
            {
                break;
            }

            render_canvas_in_block(&tile_arena,
                                   //&block_arena,
                                   milton_state->view,
                                   milton_state->cm,
                                   milton_state->strokes,
                                   stroke_masks,
                                   milton_state->num_strokes,
                                   &milton_state->working_stroke,
                                   raster_buffer,
                                   blocks[i + block_i]);
        }

        arena_pop(&tile_arena);

        ARENA_VALIDATE(milton_state->transient_arena);
    }
    return true;
}

static void render_picker(ColorPicker* picker, ColorManagement cm,
        uint32* buffer_pixels, CanvasView* view)
{
    v2f baseline = {1,0};

    v4f background_color =
    {
        0.5f,
        0.5f,
        0.55f,
        0.4f,
    };

    Rect draw_rect = picker_get_bounds(picker);

    // Copy canvas buffer into picker buffer
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            int b =
                3 * 3;
            uint32 picker_i =
                    (j - draw_rect.top) * (2*picker->bound_radius_px ) + (i - draw_rect.left);
            uint32 src = buffer_pixels[j * view->screen_size.w + i];
            picker->pixels[picker_i] = src;
        }
    }

    // Render background color.
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            uint32 picker_i =
                    (j - draw_rect.top) *( 2*picker->bound_radius_px ) + (i - draw_rect.left);
            v4f dest = color_u32_to_v4f(cm, picker->pixels[picker_i]);
            float alpha = background_color.a;
            // To gamma
            background_color.r = background_color.r * background_color.r;
            background_color.g = background_color.g * background_color.g;
            background_color.b = background_color.b * background_color.b;
            dest.r = dest.r * dest.r;
            dest.g = dest.g * dest.g;
            dest.b = dest.b * dest.b;
            // Blend and move to linear
            v4f result =
            {
                sqrtf(dest.r * (1 - alpha) + background_color.r * alpha),
                sqrtf(dest.g * (1 - alpha) + background_color.g * alpha),
                sqrtf(dest.b * (1 - alpha) + background_color.b * alpha),
                dest.a + (alpha * (1 - dest.a)),
            };
            picker->pixels[picker_i] = color_v4f_to_u32(cm, result);
        }
    }

    // render wheel
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            uint32 picker_i =
                    (j - draw_rect.top) *( 2*picker->bound_radius_px ) + (i - draw_rect.left);
            v2f point = {(float)i, (float)j};
            uint32 dest_color = picker->pixels[picker_i];

            int samples = 0;
            {
                float u = 0.223607f;
                float v = 0.670820f;

                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){-u, -v}));
                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){-v, u}));
                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){u, v}));
                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){v, u}));
            }

            if (samples > 0)
            {
                float angle = picker_wheel_get_angle(picker, point);
                float degree = radians_to_degrees(angle);
                v3f hsv = { degree, 1.0f, 1.0f };
                v3f rgb = hsv_to_rgb(hsv);

                float contrib = samples / 4.0f;

                v4f d = color_u32_to_v4f(cm, dest_color);

                // To gamma
                rgb.r = rgb.r * rgb.r;
                rgb.g = rgb.g * rgb.g;
                rgb.b = rgb.b * rgb.b;
                d.r = d.r * d.r;
                d.g = d.g * d.g;
                d.b = d.b * d.b;
                // Blend and move to linear
                v4f result =
                {
                    sqrtf(((1 - contrib) * (d.r)) + (contrib * (rgb.r))),
                    sqrtf(((1 - contrib) * (d.g)) + (contrib * (rgb.g))),
                    sqrtf(((1 - contrib) * (d.b)) + (contrib * (rgb.b))),
                    d.a + (contrib * (1 - d.a)),
                };
                uint32 color = color_v4f_to_u32(cm, result);
                picker->pixels[picker_i] = color;
            }
        }
    }

    // Render triangle
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            v2f point = { (float)i, (float)j };
            uint32 picker_i =
                    (j - draw_rect.top) *( 2*picker->bound_radius_px ) + (i - draw_rect.left);
            uint32 dest_color = picker->pixels[picker_i];
            // MSAA!!
            int samples = 0;
            {
                float u = 0.223607f;
                float v = 0.670820f;

                samples += (int)is_inside_triangle(add_v2f(point, (v2f){-u, -v}),
                        picker->a, picker->b, picker->c);
                samples += (int)is_inside_triangle(add_v2f(point, (v2f){-v, u}),
                        picker->a, picker->b, picker->c);
                samples += (int)is_inside_triangle(add_v2f(point, (v2f){u, v}),
                        picker->a, picker->b, picker->c);
                samples += (int)is_inside_triangle(add_v2f(point, (v2f){v, u}),
                        picker->a, picker->b, picker->c);
            }

            if (samples > 0)
            {
                v3f hsv = picker_hsv_from_point(picker, point);

                float contrib = samples / 4.0f;

                v4f d = color_u32_to_v4f(cm, dest_color);

                v3f rgb = hsv_to_rgb(hsv);

                /* v4f result = */
                /* { */
                /*     ((1 - contrib) * (d.r)) + (contrib * (rgb.r)), */
                /*     ((1 - contrib) * (d.g)) + (contrib * (rgb.g)), */
                /*     ((1 - contrib) * (d.b)) + (contrib * (rgb.b)), */
                /*     d.a + (contrib * (1 - d.a)), */
                /* }; */
                // To gamma
                rgb.r = rgb.r * rgb.r;
                rgb.g = rgb.g * rgb.g;
                rgb.b = rgb.b * rgb.b;
                d.r = d.r * d.r;
                d.g = d.g * d.g;
                d.b = d.b * d.b;
                // Blend and move to linear
                v4f result =
                {
                    sqrtf(((1 - contrib) * (d.r)) + (contrib * (rgb.r))),
                    sqrtf(((1 - contrib) * (d.g)) + (contrib * (rgb.g))),
                    sqrtf(((1 - contrib) * (d.b)) + (contrib * (rgb.b))),
                    d.a + (contrib * (1 - d.a)),
                };

                picker->pixels[picker_i] = color_v4f_to_u32(cm, result);
            }
        }
    }

    // Blit picker pixels
    uint32* to_blit = picker->pixels;
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            uint32 linear_color = *to_blit++;
            v4f sRGB = color_u32_to_v4f(cm, linear_color);
            uint32 color = color_v4f_to_u32(cm, sRGB);
            buffer_pixels[j * view->screen_size.w + i] = color;
        }
    }
}

typedef enum
{
    MiltonRenderFlags_none              = 0,
    MiltonRenderFlags_picker_updated    = (1 << 0),
    MiltonRenderFlags_full_redraw       = (1 << 1),
} MiltonRenderFlags;

static void milton_render(MiltonState* milton_state, MiltonRenderFlags render_flags)
{
    // `raster_limits` is the part of the screen (in pixels) that should be updated
    // with what's on the canvas.
    Rect raster_limits = { 0 };

    // Figure out what `raster_limits` should be.
    {
        if (render_flags & MiltonRenderFlags_full_redraw)
        {
            raster_limits.left = 0;
            raster_limits.right = milton_state->view->screen_size.w;
            raster_limits.top = 0;
            raster_limits.bottom = milton_state->view->screen_size.h;
        }
        else if (milton_state->working_stroke.num_points > 1)
        {
            Stroke* stroke = &milton_state->working_stroke;
            v2i new_point = canvas_to_raster(
                    milton_state->view,
                    stroke->canvas_reference,
                    stroke->points[stroke->num_points - 2]);

            raster_limits.left =   min (milton_state->last_raster_input.x, new_point.x);
            raster_limits.right =  max (milton_state->last_raster_input.x, new_point.x);
            raster_limits.top =    min (milton_state->last_raster_input.y, new_point.y);
            raster_limits.bottom = max (milton_state->last_raster_input.y, new_point.y);
            int32 block_offset = 0;
            int32 w = raster_limits.right - raster_limits.left;
            int32 h = raster_limits.bottom - raster_limits.top;
            if (w < milton_state->block_width)
            {
                block_offset = (milton_state->block_width - w) / 2;
            }
            if (h < milton_state->block_width)
            {
                block_offset = max(block_offset, (milton_state->block_width - h) / 2);
            }
            raster_limits = rect_enlarge(raster_limits,
                    block_offset + (stroke->brush.radius / milton_state->view->scale));
            raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);
            assert (raster_limits.right >= raster_limits.left);
            assert (raster_limits.bottom >= raster_limits.top);
        }
        else if (milton_state->working_stroke.num_points == 1)
        {
            Stroke* stroke = &milton_state->working_stroke;
            v2i point = canvas_to_raster(milton_state->view,
                                         stroke->canvas_reference,
                                         stroke->points[0]);
            int32 raster_radius = stroke->brush.radius / milton_state->view->scale;
            raster_radius = max(raster_radius, milton_state->block_width);
            raster_limits.left = -raster_radius  + point.x;
            raster_limits.right = raster_radius  + point.x;
            raster_limits.top = -raster_radius   + point.y;
            raster_limits.bottom = raster_radius + point.y;
            raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);
            assert (raster_limits.right >= raster_limits.left);
            assert (raster_limits.bottom >= raster_limits.top);
        }
    }

    int32 index = (milton_state->raster_buffer_index + 1) % 2;
    uint32* raster_buffer = (uint32*)milton_state->raster_buffers[index];

    bool32 completed = render_canvas(milton_state, raster_buffer, raster_limits);

    // Render UI
    if (completed)
    {
        Rect* split_rects = NULL;
        bool32 redraw = false;
        Rect picker_rect = picker_get_bounds(&milton_state->picker);
        Rect clipped = rect_intersect(picker_rect, raster_limits);
        if ((clipped.left != clipped.right) && clipped.top != clipped.bottom)
        {
            redraw = true;
        }

        if (redraw || (render_flags & MiltonRenderFlags_picker_updated))
        {
            render_canvas(milton_state, raster_buffer, picker_rect);
            render_picker(&milton_state->picker, milton_state->cm,
                          raster_buffer,
                          milton_state->view);
        }
    }

    // If not preempted, do a buffer swap.
    if (completed)
    {
        int32 prev_index = milton_state->raster_buffer_index;
        milton_state->raster_buffer_index = index;

        memcpy(milton_state->raster_buffers[prev_index],
               milton_state->raster_buffers[index],
               milton_state->max_width * milton_state->max_height * milton_state->bytes_per_pixel);
    }
}
