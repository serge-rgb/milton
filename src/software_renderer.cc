// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Declared here so that the workers get launched from the init function.
int renderer_worker_thread(/* WorkerParams* */void* data);


struct WorkerParams
{
    MiltonState* milton_state;
    i32 worker_id;
};


// Special values for ClippedStroke.num_points
enum ClippedStrokeFlags
{
    ClippedStroke_IS_LAYERMARK = -2,
};

// Always accessed together. Keeping them sequential to save some cache misses.
typedef struct ClippedPoint
{
    i32 x;
    i32 y;
    f32 pressure;
} ClippedPoint;

typedef struct ClippedStroke ClippedStroke;
struct ClippedStroke
{
    // Same contents as a stroke, but these points are stored in relative
    // coordinates to the center of the block. This helps with FP precision. We
    // create them per block. Each block gets an array of ClippedStroke which
    // are likely to fill pixels.
    //
    // We store n*2-2 where n is the original number of points. They are stored
    // in segment form, eg. AB BC CD DE. It's faster to treat points as
    // segments. Single-point strokes are single point clipped strokes.
    ClippedPoint*   clipped_points;
    Brush           brush;

    // A clipped stroke is never empty. We use the negative values as flags.
    // See ClippedStrokeFlags
    i32             num_points;

    // Point data for segments AB BC CD DE etc..
    // Yes, it is a linked list. Shoot me.
    //
    // Cache misses are not a problem in the stroke rasterization path.
    ClippedStroke*  next;
};

static b32 clipped_stroke_is_layermark(ClippedStroke* clipped_stroke)
{
    b32 is_mark = false;
    if ( clipped_stroke )
    {
        is_mark = (clipped_stroke->num_points == ClippedStroke_IS_LAYERMARK);
    }
    return is_mark;
}

// Clipped stroke points are stored relative to the reference point.
static ClippedStroke* stroke_clip_to_rect(Arena* render_arena, Stroke* in_stroke,
                                          Rect canvas_rect, i32 local_scale, v2i reference_point)
{
    ClippedStroke* clipped_stroke = arena_alloc_elem(render_arena, ClippedStroke);

    if (!clipped_stroke)
    {
        return NULL;
    }

    clipped_stroke->num_points = 0;
    clipped_stroke->next  = NULL;
    clipped_stroke->brush = in_stroke->brush;

    i32 points_allocated = in_stroke->num_points * 2 + 8;

    // ... now substitute the point data with an array of our own.
    if (in_stroke->num_points > 0)
    {
        // Add enough zeroed-out points to align the arrays to a multiple of 4
        // points so that the renderer can comfortably load from the arrays
        // without bounds checking.

        clipped_stroke->clipped_points = arena_alloc_array(render_arena, points_allocated, ClippedPoint);
        //memset(clipped_stroke->clipped_points, 0, points_allocated * sizeof(ClippedPoint));
    }
    else
    {
        milton_log("WARNING: An empty stroke was received to clip.\n");
        return clipped_stroke;
    }

    if ( !clipped_stroke->clipped_points)
    {
        // We need more memory. Return gracefully
        return NULL;
    }
    if ( in_stroke->num_points == 1 )
    {
        if (is_inside_rect(canvas_rect, in_stroke->points[0]))
        {
            clipped_stroke->num_points = 1;
            clipped_stroke->clipped_points[0].x = in_stroke->points[0].x * local_scale - reference_point.x;
            clipped_stroke->clipped_points[0].y = in_stroke->points[0].y * local_scale - reference_point.y;
            clipped_stroke->clipped_points[0].pressure = in_stroke->pressures[0];
        }
    }
    else if ( in_stroke->num_points > 1 )
    {
        i32 num_points = in_stroke->num_points;
        for (i32 point_i = 0; point_i < num_points - 1; ++point_i)
        {
            v2i a = in_stroke->points[point_i];
            v2i b = in_stroke->points[point_i + 1];

            // Very conservative...
            b32 maybe_inside = !((a.x > canvas_rect.right  && b.x > canvas_rect.right) ||
                                 (a.x < canvas_rect.left   && b.x < canvas_rect.left) ||
                                 (a.y < canvas_rect.top    && b.y < canvas_rect.top) ||
                                 (a.y > canvas_rect.bottom && b.y > canvas_rect.bottom));

            // We can add the segment
            if ( maybe_inside )
            {
                clipped_stroke->clipped_points[clipped_stroke->num_points].x     = a.x * local_scale - reference_point.x;
                clipped_stroke->clipped_points[clipped_stroke->num_points].y     = a.y * local_scale - reference_point.y;
                clipped_stroke->clipped_points[clipped_stroke->num_points + 1].x = b.x * local_scale - reference_point.x;
                clipped_stroke->clipped_points[clipped_stroke->num_points + 1].y = b.y * local_scale - reference_point.y;

                f32 pressure_a = in_stroke->pressures[point_i];
                f32 pressure_b = in_stroke->pressures[point_i + 1];

                clipped_stroke->clipped_points[clipped_stroke->num_points].pressure     = pressure_a;
                clipped_stroke->clipped_points[clipped_stroke->num_points + 1].pressure = pressure_b;

                clipped_stroke->num_points += 2;
            }
        }
    }
    else
    {
        // We should have already handled the pathological case of the empty stroke.
        INVALID_CODE_PATH;
    }


    // SSE loop will eat up extra items to avoid a conditional, it expects zero'ed data.
    memset(clipped_stroke->clipped_points + clipped_stroke->num_points, 0, 8 * sizeof(ClippedPoint));

    return clipped_stroke;
}

// Returns a linked list of strokes that this block needs to render.
static ClippedStroke* clip_strokes_to_block(Arena* render_arena,
                                            i32 worker_id,
                                            Layer* root_layer,
                                            Stroke* working_stroke,
                                            Rect canvas_block,
                                            i32 local_scale,
                                            v2i reference_point,
                                            b32* allocation_ok)
{
    ClippedStroke* stroke_list = NULL;
    *allocation_ok = true;
    Layer* layer = root_layer;
    while ( layer )
    {
        if ( !(layer->flags & LayerFlags_VISIBLE) )
        {
            layer = layer->next;
            continue;
        }

        if ( layer != root_layer && clipped_stroke_is_layermark(stroke_list) == false )
        {
            ClippedStroke* layer_mark = arena_alloc_elem(render_arena, ClippedStroke);
            if ( !layer_mark )
            {
                *allocation_ok = false;
            }
            else
            {
                *layer_mark = {};
                layer_mark->num_points = ClippedStroke_IS_LAYERMARK;
                layer_mark->next = stroke_list;
                stroke_list = layer_mark;
            }
        }

        Stroke* strokes = layer->strokes.data;
        u64 num_strokes = layer->strokes.count;

        // Fill linked list with strokes clipped to this block
        if ( allocation_ok )
        {
            for (u64 stroke_i = 0; stroke_i <= num_strokes; ++stroke_i)
            {
                // Sum of strokes before this layer.
                size_t total_stroke_i = stroke_i;
                for ( Layer* l=root_layer; l!=layer; l=l->next )
                {
                    if (l->flags & LayerFlags_VISIBLE)
                    {
                        total_stroke_i += l->strokes.count;
                    }
                }

                Stroke* unclipped_stroke = NULL;
                if ( stroke_i == num_strokes )
                {
                    if (layer->id == working_stroke->layer_id && working_stroke->num_points)
                    {  // Topmost layer: Use working stroke
                        unclipped_stroke = working_stroke;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    unclipped_stroke = &strokes[stroke_i];
                }
                assert(unclipped_stroke);

                if (unclipped_stroke->visibility[worker_id] == false)
                {
                    // Skip strokes that have been clipped.
                    continue;
                }

                {
                    Rect enlarged_block = rect_enlarge(canvas_block, unclipped_stroke->brush.radius);
                    ClippedStroke* clipped_stroke = stroke_clip_to_rect(render_arena, unclipped_stroke,
                                                                        enlarged_block, local_scale, reference_point);
                    // ALlocation failed.
                    // Handle this gracefully; this will result in asking for more memory for render workers.
                    if ( !clipped_stroke )
                    {
                        *allocation_ok = false;
                        return NULL;
                    }

                    if ( clipped_stroke->num_points > 0 )
                    {  // Empty strokes ignored.
                        ClippedStroke* list_head = clipped_stroke;

                        list_head->next = stroke_list;
                        stroke_list = list_head;
                    }
                }
            }
        }

        layer = layer->next;
    }
    return stroke_list;
}

// Receives a stretchy array of strokes and rasterizes it to the specified block of pixels.
// Returns false if allocation failed.
static b32 rasterize_canvas_block_slow(Arena* render_arena,
                                       i32 worker_id,
                                       CanvasView* view,
                                       Layer* root_layer,
                                       Stroke* working_stroke,
                                       u32* pixels,
                                       Rect raster_block)
{
    Rect canvas_block;
    {
        canvas_block.top_left  = raster_to_canvas(view, raster_block.top_left);
        canvas_block.bot_right = raster_to_canvas(view, raster_block.bot_right);
    }

    if ( canvas_block.left   < -view->canvas_radius_limit ||
         canvas_block.right  > view->canvas_radius_limit  ||
         canvas_block.top    < -view->canvas_radius_limit ||
         canvas_block.bottom > view->canvas_radius_limit )
    {
        for ( int j = raster_block.top; j < raster_block.bottom; j++ )
        {
            for ( int i = raster_block.left; i < raster_block.right; i++ )
            {
                pixels[j * view->screen_size.w + i] = 0xffff00ff;
            }
        }
        return true;
    }

// Leaving this toggle-able for a quick way to show the cool precision error.
    v2i reference_point =
#if 0
    {
        (canvas_block.left + canvas_block.right) / 2,
        (canvas_block.top + canvas_block.bottom) / 2,
    };
#else
    {0};
#endif

    // Get the linked list of clipped strokes.
    b32 allocation_ok = true;
    i32 local_scale =  (view->scale <= 8) ?  4 : 1;
    {
        reference_point.x *= local_scale;
        reference_point.y *= local_scale;
    }
    ClippedStroke* stroke_list = clip_strokes_to_block(render_arena,
                                                       worker_id,
                                                       root_layer,
                                                       working_stroke,
                                                       canvas_block, local_scale, reference_point,
                                                       &allocation_ok);


    if (!allocation_ok)
    {
        // Request more memory
        return false;
    }

    i32 downsample_factor = view->downsampling_factor;

    // This is the distance between two adjacent canvas pixels. Derived to
    // avoid expensive raster_to_canvas computations in the inner loop
    i32 canvas_jump = downsample_factor * view->scale * local_scale;


    // i and j are the canvas point
    i32 j = (((raster_block.top - view->screen_center.y) *
              view->scale) - view->pan_vector.y) * local_scale - reference_point.y;

    for ( i32 pixel_j = raster_block.top;
          pixel_j < raster_block.bottom;
          pixel_j += downsample_factor )
    {
        i32 i =  (((raster_block.left - view->screen_center.x) *
                    view->scale) - view->pan_vector.x) * local_scale - reference_point.x;

        for ( i32 pixel_i = raster_block.left;
              pixel_i < raster_block.right;
              pixel_i += downsample_factor )
        {
            // Clear color
            v4f background_color;
            background_color.rgb = view->background_color;
            background_color.a = 1.0f;

            // Cumulative blending
            v4f acc_color = { 0 };

            ClippedStroke* list_iter = stroke_list;
            b32 pixel_erased = false;

            while (list_iter)
            {
                ClippedStroke* clipped_stroke = list_iter;
                list_iter = list_iter->next;

                if (clipped_stroke_is_layermark(clipped_stroke))
                {
                    pixel_erased = false;
                    continue;
                }
                else if (pixel_erased)
                {
                    continue;
                }

                b32 iseraser = is_eraser(&clipped_stroke->brush);

                ClippedPoint* points = clipped_stroke->clipped_points;

                f32 min_dist = FLT_MAX;
                f32 dx = 0;
                f32 dy = 0;
                f32 pressure = 0.0f;

                if (clipped_stroke->num_points == 1)
                {
                    dx = (f32)(i - points[0].x);
                    dy = (f32)(j - points[0].y);
                    min_dist = dx * dx + dy * dy;
                    pressure = points[0].pressure;
                }
                else
                {
                    // Find closest point.
                    for (int point_i = 0; point_i < clipped_stroke->num_points - 1; point_i += 2)
                    {
                        i32 ax = points[point_i].x;
                        i32 ay = points[point_i].y;
                        i32 bx = points[point_i + 1].x;
                        i32 by = points[point_i + 1].y;
                        f32 p_a = points[point_i    ].pressure;
                        f32 p_b = points[point_i + 1].pressure;

                        v2f ab = {(f32)(bx - ax), (f32)(by - ay)};
                        f32 mag_ab2 = ab.x * ab.x + ab.y * ab.y;
                        if ( mag_ab2 > 0 )
                        {
                            f32 t;
                            v2f point = closest_point_in_segment_f(ax, ay, bx, by,
                                                                   ab, mag_ab2,
                                                                   {i,j}, &t);
                            f32 test_dx = (f32) (i - point.x);
                            f32 test_dy = (f32) (j - point.y);
                            f32 dist = sqrtf(test_dx * test_dx + test_dy * test_dy);
                            f32 test_pressure = (1 - t) * p_a + t * p_b;
                            dist = dist - test_pressure * clipped_stroke->brush.radius * local_scale;
                            if (dist < min_dist)
                            {
                                min_dist = dist;
                                dx = test_dx;
                                dy = test_dy;
                                pressure = test_pressure;
                            }
                        }
                    }
                }

                if (min_dist < FLT_MAX)
                {
                    // TODO: For implicit brush:
                    //  This sampling is for a circular brush.
                    //  Should dispatch on brush type. And do it for SSE impl too.
                    int samples = 0;
                    f32 f3 = (0.75f * view->scale) * downsample_factor * local_scale;
                    f32 f1 = (0.25f * view->scale) * downsample_factor * local_scale;
                    u32 radius = (u32)(clipped_stroke->brush.radius * pressure * local_scale);
                    f32 fdists[16];
                    {
                        f32 a1 = (dx - f3) * (dx - f3);
                        f32 a2 = (dx - f1) * (dx - f1);
                        f32 a3 = (dx + f1) * (dx + f1);
                        f32 a4 = (dx + f3) * (dx + f3);

                        f32 b1 = (dy - f3) * (dy - f3);
                        f32 b2 = (dy - f1) * (dy - f1);
                        f32 b3 = (dy + f1) * (dy + f1);
                        f32 b4 = (dy + f3) * (dy + f3);

                        fdists[0]  = a1 + b1;
                        fdists[1]  = a2 + b1;
                        fdists[2]  = a3 + b1;
                        fdists[3]  = a4 + b1;

                        fdists[4]  = a1 + b2;
                        fdists[5]  = a2 + b2;
                        fdists[6]  = a3 + b2;
                        fdists[7]  = a4 + b2;

                        fdists[8]  = a1 + b3;
                        fdists[9]  = a2 + b3;
                        fdists[10] = a3 + b3;
                        fdists[11] = a4 + b3;

                        fdists[12] = a1 + b4;
                        fdists[13] = a2 + b4;
                        fdists[14] = a3 + b4;
                        fdists[15] = a4 + b4;
                    }

                    // Perf note: We remove the sqrtf call when it's
                    // safe to square the radius
                    if (radius >= ( 1 << 16 ))
                    {
                        for (int sample_i = 0; sample_i < 16; ++sample_i)
                        {
                            samples += (sqrtf(fdists[sample_i]) < radius);
                        }
                    }
                    else
                    {
                        u32 sq_radius = radius * radius;

                        for (int sample_i = 0; sample_i < 16; ++sample_i)
                        {
                            samples += (fdists[sample_i] < sq_radius);
                        }
                    }

                    // If the stroke contributes to the pixel, do compositing.
                    if (samples > 0)
                    {
                        // Do blending
                        // ---------------

                        if (iseraser)
                        {
                            pixel_erased = true;
                        }
                        else
                        {
                            f32 coverage = (f32)samples / 16.0f;

                            //v4f dst = is_eraser? background_color : clipped_stroke->brush.color;
                            v4f dst = clipped_stroke->brush.color;
                            {
                                dst.r *= coverage;
                                dst.g *= coverage;
                                dst.b *= coverage;
                                dst.a *= coverage;
                            }

                            acc_color = blend_v4f(dst, acc_color);
                        }
                    }
                }

                // This pixel is done if alpha == 1. This is is why stroke_list is reversed.
                if (acc_color.a >= 1.0f)
                {
                    break;
                }
            }

            // Blend onto the background whatever is accumulated.
            acc_color = blend_v4f(background_color, acc_color);

            // From [0, 1] to [0, 255]
            u32 pixel = color_v4f_to_u32(acc_color);

            for (i32 jj = pixel_j; jj < pixel_j + downsample_factor; ++jj)
            {
                for (i32 ii = pixel_i; ii < pixel_i + downsample_factor; ++ii)
                {
                    pixels[jj * view->screen_size.w + ii] = pixel;
                }
            }
            i += canvas_jump;
        }
        j += canvas_jump;
    }

    return true;
}


// For a more readable implementation that does the same thing. See rasterize_canvas_block_slow.
static b32 rasterize_canvas_block_sse2(Arena* render_arena,
                                       i32 worker_id,
                                       CanvasView* view,
                                       Layer* root_layer,
                                       Stroke* working_stroke,
                                       u32* pixels,
                                       Rect raster_block)
{
    PROFILE_RASTER_BEGIN(sse2);
    PROFILE_RASTER_BEGIN(preamble);

    __m128 one4 = _mm_set_ps1(1);
    __m128 zero4 = _mm_set_ps1(0);


    Rect canvas_block;
    {
        canvas_block.top_left  = raster_to_canvas(view, raster_block.top_left);
        canvas_block.bot_right = raster_to_canvas(view, raster_block.bot_right);
    }

    if (canvas_block.left   < -view->canvas_radius_limit ||
        canvas_block.right  > view->canvas_radius_limit  ||
        canvas_block.top    < -view->canvas_radius_limit ||
        canvas_block.bottom > view->canvas_radius_limit)
    {
        for (int j = raster_block.top; j < raster_block.bottom; j++)
        {
            for (int i = raster_block.left; i < raster_block.right; i++)
            {
                pixels[j * view->screen_size.w + i] = 0xffff00ff;
            }
        }
        return true;
    }

    v2i reference_point =
// Leaving this toggle-able for a quick way to show the cool precision error.
#if 1
    {
        (canvas_block.left + canvas_block.right) / 2,
        (canvas_block.top + canvas_block.bottom) / 2,
    };
#else
    {0};
#endif

    // Get the linked list of clipped strokes.
    b32 allocation_ok = true;
    i32 local_scale =  (view->scale <= 8) ?  4 : 1;

    __m128 local_scale4 = _mm_set_ps1((float)local_scale);
    {
        reference_point.x *= local_scale;
        reference_point.y *= local_scale;
    }
    ClippedStroke* stroke_list = clip_strokes_to_block(render_arena,
                                                       worker_id,
                                                       root_layer,
                                                       working_stroke,
                                                       canvas_block, local_scale, reference_point,
                                                       &allocation_ok);

    if ( !allocation_ok )
    {
        // Request more memory
        return false;
    }

    i32 downsample_factor = view->downsampling_factor;

    // This is the distance between two adjacent canvas pixels. Derived to
    // avoid expensive raster_to_canvas computations in the inner loop
    i32 canvas_jump = downsample_factor * view->scale * local_scale;


    PROFILE_RASTER_PUSH(preamble);
    PROFILE_RASTER_BEGIN(total_work_loop);

    // Clear color
    v4f background_color;

    background_color.rgb = view->background_color;
    background_color.a = 1.0f;



    // i and j are the canvas point
    i32 j = (((raster_block.top - view->screen_center.y) *
              view->scale) - view->pan_vector.y) * local_scale - reference_point.y;

    for ( i32 pixel_j = raster_block.top;
          pixel_j < raster_block.bottom;
          pixel_j += downsample_factor )
    {
        i32 i = (((raster_block.left - view->screen_center.x) *
                  view->scale) - view->pan_vector.x) * local_scale - reference_point.x;

        for ( i32 pixel_i = raster_block.left;
              pixel_i < raster_block.right;
              pixel_i += downsample_factor )
        {
            // Cumulative blending
            v4f acc_color = { 0 };

            ClippedStroke* list_iter = stroke_list;
            b32 pixel_erased = false;

            while( list_iter )
            {
                ClippedStroke* clipped_stroke = list_iter;
                list_iter = list_iter->next;

                if ( clipped_stroke_is_layermark(clipped_stroke) )
                {
                    pixel_erased = false;
                    continue;
                }
                else if ( pixel_erased )
                {
                    continue;
                }

                b32 iseraser = is_eraser(&clipped_stroke->brush);

                // Slow path. There are pixels not inside.
                ClippedPoint* points = clipped_stroke->clipped_points;

                f32 min_dist = FLT_MAX;
                f32 dx = 0;
                f32 dy = 0;
                f32 pressure = 0.0f;

                if ( clipped_stroke->num_points == 1 )
                {
                    dx = (f32)(i - points[0].x);
                    dy = (f32)(j - points[0].y);
                    min_dist = dx * dx + dy * dy;
                    pressure = points[0].pressure;
                }
                else
                {
                    //#define SSE_M(wide, i) ((f32 *)&(wide) + i)
                    for (int point_i = 0; point_i < clipped_stroke->num_points - 1; point_i += (2 * 4) )
                    {
                        // The step is 4 (128 bit SIMD) times 2 (points are in format AB BC CD DE)

                        PROFILE_RASTER_BEGIN(load);

                        ALIGN(16) f32 axs[4];
                        ALIGN(16) f32 ays[4];
                        ALIGN(16) f32 bxs[4];
                        ALIGN(16) f32 bys[4];
                        ALIGN(16) f32 aps[4];
                        ALIGN(16) f32 bps[4];

                        // NOTE: This loop is not stupid:
                        //  I transformed the data representation to SOA
                        //  form. There was no measurable difference even
                        //  though this loop turned into 4 _mm_load_ps
                        //  intrinsics.
                        //
                        // We can comfortably get 4 elements because the
                        // points are allocated with enough trailing zeros.
                        i32 l_point_i = point_i;
                        for ( i32 batch_i = 0; batch_i < 4; batch_i++ )
                        {
                            i32 index = l_point_i + batch_i;

                            // The point of reference point is to do the subtraction with
                            // integer arithmetic
                            axs[batch_i] = (f32)(points[index  ].x);
                            bxs[batch_i] = (f32)(points[index+1].x);
                            ays[batch_i] = (f32)(points[index  ].y);
                            bys[batch_i] = (f32)(points[index+1].y);
                            aps[batch_i] = points[index  ].pressure;
                            bps[batch_i] = points[index+1].pressure;

                            l_point_i += 1;
                        }

                        __m128 a_x        = _mm_load_ps(axs);
                        __m128 a_y        = _mm_load_ps(ays);
                        __m128 b_x        = _mm_load_ps(bxs);
                        __m128 b_y        = _mm_load_ps(bys);
                        __m128 pressure_a = _mm_load_ps(aps);
                        __m128 pressure_b = _mm_load_ps(bps);

                        PROFILE_RASTER_PUSH(load);
                        PROFILE_RASTER_BEGIN(work);

                        __m128 ab_x = _mm_sub_ps(b_x, a_x);
                        __m128 ab_y = _mm_sub_ps(b_y, a_y);

                        __m128 mag_ab2 = _mm_add_ps(_mm_mul_ps(ab_x, ab_x),
                                                    _mm_mul_ps(ab_y, ab_y));

                        // mask out the values for which we will divide-by-zero
                        __m128 mask = _mm_cmpgt_ps(mag_ab2, zero4);

                        // Inverse magnitude of ab
                        __m128 inv_mag_ab = _mm_rsqrt_ps(mag_ab2);

                        __m128 d_x = _mm_mul_ps(ab_x, inv_mag_ab);
                        __m128 d_y = _mm_mul_ps(ab_y, inv_mag_ab);

                        __m128 canvas_point_x4 = _mm_set_ps1((f32)i);
                        __m128 canvas_point_y4 = _mm_set_ps1((f32)j);

                        __m128 ax_x = _mm_sub_ps(canvas_point_x4, a_x);
                        __m128 ax_y = _mm_sub_ps(canvas_point_y4, a_y);

                        __m128 disc = _mm_add_ps(_mm_mul_ps(d_x, ax_x),
                                                 _mm_mul_ps(d_y, ax_y));

                        // Clamp discriminant so that point lies in [a, b]
                        {
                            __m128 low  = _mm_set_ps1(0);
                            __m128 high = _mm_div_ps(one4, inv_mag_ab);

                            disc = _mm_min_ps(_mm_max_ps(low, disc), high);
                        }

                        // Between 0 and 1 in the AB segment
                        __m128 t4 = _mm_mul_ps(disc, inv_mag_ab);


                        // point_x is the closest point in the AB segment to the canvas point
                        // corresponding to the i,j point.
                        //
                        // (axs[i] + disc_i * (d_x[i],
                        __m128 point_x_4 = _mm_add_ps(a_x, _mm_mul_ps(disc, d_x));
                        __m128 point_y_4 = _mm_add_ps(a_y, _mm_mul_ps(disc, d_y));

                        __m128 test_dx = _mm_sub_ps(canvas_point_x4, point_x_4);
                        __m128 test_dy = _mm_sub_ps(canvas_point_y4, point_y_4);

                        __m128 dist4 = _mm_add_ps(_mm_mul_ps(test_dx, test_dx),
                                                  _mm_mul_ps(test_dy, test_dy));
                        dist4 = _mm_sqrt_ps(dist4);

                        // Lerp
                        // (1 - t) * p_a + t * p_b;
                        __m128 pressure4 = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one4, t4),
                                                                 pressure_a),
                                                      _mm_mul_ps(t4,
                                                                 pressure_b));

                        __m128 radius4 = _mm_set_ps1((f32)clipped_stroke->brush.radius);
                        dist4 = _mm_sub_ps(dist4, _mm_mul_ps(_mm_mul_ps(pressure4, radius4),
                                                             local_scale4));

                        PROFILE_RASTER_PUSH(work);
                        PROFILE_RASTER_BEGIN(gather);

                        ALIGN(16) f32 dists[4];
                        ALIGN(16) f32 tests_dx[4];
                        ALIGN(16) f32 tests_dy[4];
                        ALIGN(16) f32 pressures[4];
                        ALIGN(16) f32 masks[4];

                        _mm_store_ps(dists, dist4);
                        _mm_store_ps(tests_dx, test_dx);
                        _mm_store_ps(tests_dy, test_dy);
                        _mm_store_ps(pressures, pressure4);
                        _mm_store_ps(masks, mask);

                        // Two versions of the "gather" step. A "smart" version (in quotes
                        // because there is probably a better, smarter way of doing it) and a
                        // "dumb" naive loop.  The dumb loop is just slightly faster. Here is
                        // the output from a test run, with a very intricate drawing:
                        //

                        /**
                          (2016-02-14) Note that the gather step gets faster, and the inner loop
                          measuerments don't change that much. This may have something to do
                          with the dumb loop freeing the SIMD unit for other work? No idea.

                          tl;dr : "dumb" loop is 1.08 times faster.





                        SMART

                          ===== Profiler output ==========
                        render_canvas:   ncalls:       10,          clocks_per_call:      2101567716
                        sse2:            ncalls:       10000,       clocks_per_call:        17619835
                        preamble:        ncalls:       10000,       clocks_per_call:          200762
                        load:            ncalls:       453752510,   clocks_per_call:              76
                        work:            ncalls:       453752510,   clocks_per_call:              87
                        gather:          ncalls:       453752510,   clocks_per_call:              45
                        sampling:        ncalls:       237962700,   clocks_per_call:              99
                        total_work_loop: ncalls:       10000,       clocks_per_call:        17418932
                        ================================

                        DUMB

                        ===== Profiler output ==========
                        render_canvas:   ncalls:       10,          clocks_per_call:      1938976982
                        sse2:            ncalls:       10000,       clocks_per_call:        17452576
                        preamble:        ncalls:       10000,       clocks_per_call:          204913
                        load:            ncalls:       453752510,   clocks_per_call:              77
                        work:            ncalls:       453752510,   clocks_per_call:              69
                        gather:          ncalls:       453752510,   clocks_per_call:              54
                        sampling:        ncalls:       237962700,   clocks_per_call:             103
                        total_work_loop: ncalls:       10000,       clocks_per_call:        17247544
                        ================================
                         **/
#if 0
                        __m128 max_pos4 = _mm_set_ps1(FLT_MAX);
                        __m128 min_neg4 = _mm_set_ps1(-FLT_MAX);
                        __m128 maxed    = _mm_andnot_ps(mask, max_pos4);
                        __m128 minned   = _mm_and_ps(mask, min_neg4);

                        dist4 = _mm_max_ps(dist4, _mm_xor_ps(maxed, minned));
                        // dist = [a, b, c, d]
                        __m128 m0 = _mm_shuffle_ps(dist4, dist4, 0x71); // m0   = [b, x, d, x]
                        m0 = _mm_min_ps(dist4, m0);  // m0 = [min(a, b), x, min(c, d), x]
                        __m128 m1 = _mm_shuffle_ps(m0, m0, 2);  // m1 = [min(c, d), x, x, x]
                        // min4 [ (min(min(a, b), min(c, d))) x x x]
                        __m128 min4 = _mm_min_ps(m0, m1);
                        f32 dist = _mm_cvtss_f32(min4);
                        if (dist < min_dist)
                        {
                            int bit  = _mm_movemask_ps(_mm_cmpeq_ps(_mm_set_ps1(dist), dist4));
                            int batch_i = -1;
#ifdef _WIN32
                            _BitScanForward64((DWORD*)&batch_i, bit);
#else  // TODO: in clang&gcc: __builtin_ctz
                            for (int p = 0; p < 4; ++p)
                            {
                                if ( bit & (1 << p) )
                                {
                                    batch_i = p;
                                    break;
                                }
                            }
#endif // _WIN32
                            min_dist = dist;
                            dx = tests_dx[batch_i];
                            dy = tests_dy[batch_i];
                            pressure = pressures[batch_i];
                        }

#else
                        for ( i32 batch_i = 0; batch_i < 4; ++batch_i )
                        {
                            f32 dist = dists[batch_i];
                            i32 imask = *(i32*)&masks[batch_i];
                            if (dist < min_dist && imask == -1)
                            {
                                min_dist = dist;
                                dx = tests_dx[batch_i];
                                dy = tests_dy[batch_i];
                                pressure = pressures[batch_i];
                            }
                        }
#endif
                        PROFILE_RASTER_PUSH(gather);
                    }
                }

                PROFILE_RASTER_BEGIN(sampling);

                if ( min_dist < FLT_MAX )
                {
                    //u64 kk_ccount_begin = __rdtsc();
                    u32 samples = 0;
                    {
                        f32 f3 = (0.75f * view->scale) * downsample_factor * local_scale;
                        f32 f1 = (0.25f * view->scale) * downsample_factor * local_scale;
                        u32 radius = (u32)(clipped_stroke->brush.radius * pressure * local_scale);
                        __m128 dists[4];

                        {
                            __m128 a = _mm_set_ps((dx + f3) * (dx + f3),
                                                  (dx + f1) * (dx + f1),
                                                  (dx - f1) * (dx - f1),
                                                  (dx - f3) * (dx - f3));

                            __m128 b1 = _mm_set_ps1((dy - f3) * (dy - f3));
                            __m128 b2 = _mm_set_ps1((dy - f1) * (dy - f1));
                            __m128 b3 = _mm_set_ps1((dy + f1) * (dy + f1));
                            __m128 b4 = _mm_set_ps1((dy + f3) * (dy + f3));

                            dists[0] = _mm_add_ps(a, b1);
                            dists[1] = _mm_add_ps(a, b2);
                            dists[2] = _mm_add_ps(a, b3);
                            dists[3] = _mm_add_ps(a, b4);
                        }
                        //u32 radius = clipped_stroke->brush.radius;
                        //assert (radius > 0);
                        assert (radius < sqrtf((FLT_MAX)));

                        __m128 radius4 = _mm_set_ps1((f32)radius);

                        // Perf note: We remove the sqrtf call when it's
                        // safe to square the radius
                        __m128 comparisons[4];
                        __m128 ones = _mm_set_ps1(1.0f);
                        if (radius >= (1 << 16))
                        {
                            // sqrt slow. rsqrt fast
                            dists[0] = _mm_mul_ps(dists[0], _mm_rsqrt_ps(dists[0]));
                            dists[1] = _mm_mul_ps(dists[1], _mm_rsqrt_ps(dists[1]));
                            dists[2] = _mm_mul_ps(dists[2], _mm_rsqrt_ps(dists[2]));
                            dists[3] = _mm_mul_ps(dists[3], _mm_rsqrt_ps(dists[3]));
                            comparisons[0] = _mm_cmplt_ps(dists[0], radius4);
                            comparisons[1] = _mm_cmplt_ps(dists[1], radius4);
                            comparisons[2] = _mm_cmplt_ps(dists[2], radius4);
                            comparisons[3] = _mm_cmplt_ps(dists[3], radius4);
                        }
                        else
                        {
                            __m128 sq_radius = _mm_mul_ps(radius4, radius4);
                            comparisons[0] = _mm_cmplt_ps(dists[0], sq_radius);
                            comparisons[1] = _mm_cmplt_ps(dists[1], sq_radius);
                            comparisons[2] = _mm_cmplt_ps(dists[2], sq_radius);
                            comparisons[3] = _mm_cmplt_ps(dists[3], sq_radius);
                        }
                        __m128 sum = _mm_set_ps1(0);
                        sum = _mm_add_ps(sum, _mm_and_ps(ones, comparisons[0]));
                        sum = _mm_add_ps(sum, _mm_and_ps(ones, comparisons[1]));
                        sum = _mm_add_ps(sum, _mm_and_ps(ones, comparisons[2]));
                        sum = _mm_add_ps(sum, _mm_and_ps(ones, comparisons[3]));

                        sum = _mm_add_ps(sum, _mm_movehl_ps(sum, sum));
                        sum = _mm_add_ss(sum, _mm_shuffle_ps(sum, sum, 1));

                        float fsamples;
                        _mm_store_ss(&fsamples, sum);
                        samples = (u32)fsamples;
                    }

                    // If the stroke contributes to the pixel, do compositing.
                    if (samples > 0)
                    {
                        // Do blending
                        // ---------------

                        if (iseraser)
                        {
                            pixel_erased = true;
                        }
                        else
                        {
                            f32 coverage = (f32)samples / 16.0f;

                            v4f dst = clipped_stroke->brush.color;
                            {
                                dst.r *= coverage;
                                dst.g *= coverage;
                                dst.b *= coverage;
                                dst.a *= coverage;
                            }

                            acc_color = blend_v4f(dst, acc_color);
                        }
                    }
                }
                PROFILE_RASTER_PUSH(sampling);

                // This pixel is done if alpha == 1. This is is why stroke_list is reversed.
                if ( acc_color.a >= 1.0f )
                {
                    break;
                }
            }

            // Blend onto the background whatever is accumulated.
            acc_color = blend_v4f(background_color, acc_color);

            // From [0, 1] to [0, 255]
            u32 pixel = color_v4f_to_u32(acc_color);

            for ( i32 jj = pixel_j; jj < pixel_j + downsample_factor; ++jj )
            {
                for ( i32 ii = pixel_i; ii < pixel_i + downsample_factor; ++ii )
                {
                    pixels[jj * view->screen_size.w + ii] = pixel;
                }
            }

            i += canvas_jump;
        }
        j += canvas_jump;
    }
    PROFILE_RASTER_PUSH(total_work_loop);
    PROFILE_RASTER_PUSH(sse2);
    return true;
}

static void draw_ring(u32* pixels,
                      i32 width, i32 height,
                      i32 center_x, i32 center_y,
                      i32 ring_radius, i32 ring_girth,
                      v4f color)
{
#define COMPARE(dist) \
    dist < SQUARE(ring_radius + ring_girth) && \
    dist > SQUARE(ring_radius - ring_girth)
#define DISTANCE(i, j) \
    ((i) - center_x) * ((i) - center_x) + ((j) - center_y) * ((j) - center_y)

    assert(ring_radius < (1 << 16));

    i32 left = max(center_x - ring_radius - ring_girth, 0);
    i32 right = min(center_x + ring_radius + ring_girth, width);
    i32 top = max(center_y - ring_radius - ring_girth, 0);
    i32 bottom = min(center_y + ring_radius + ring_girth, height);

    for ( i32 j = top; j <= bottom; ++j )
    {
        for ( i32 i = left; i <= right; ++i )
        {
            // Rotated grid AA
            int samples = 0;
            {
                // Rotated 2x2 grid
                //  The u,v magic values are related to a 2d rotation by some angle,
                //  I should have commented this before forgetting
                //        ()--------()
                //       /         /
                //     /         /
                //   ()--------()
                f32 u = 0.223607f;
                f32 v = 0.670820f;

                f32 fi = (f32)i;
                f32 fj = (f32)j;

                samples += COMPARE(DISTANCE(fi - u, fj - v));
                samples += COMPARE(DISTANCE(fi - v, fj + u));
                samples += COMPARE(DISTANCE(fi + u, fj + v));
                samples += COMPARE(DISTANCE(fi + v, fj + u));
            }

            if ( samples > 0 )
            {
                f32 contrib = (f32)samples / 4.0f;
                v4f aa_color = to_premultiplied(color.rgb, contrib * color.a);
                v4f dst = color_u32_to_v4f(pixels[j*width + i]);
                v4f blended = blend_v4f(dst, aa_color);
                pixels[j*width + i] = color_v4f_to_u32(blended);
            }
        }
    }
#undef compare
#undef distance
}

static void draw_circle(u32* raster_buffer,
                        i32 raster_buffer_width, i32 raster_buffer_height,
                        i32 center_x, i32 center_y,
                        i32 radius,
                        v4f src_color)
{
    i32 left   = max(center_x - radius, 0);
    i32 right  = min(center_x + radius, raster_buffer_width);
    i32 top    = max(center_y - radius, 0);
    i32 bottom = min(center_y + radius, raster_buffer_height);

    if((right >= left) &&(bottom >= top))
    {
        for ( i32 j = top; j <= bottom; ++j )
        {
            for ( i32 i = left; i <= right; ++i )
            {
                i32 index = j * raster_buffer_width + i;

                //TODO: AA
                f32 dist = distance({(f32)i, (f32)j },
                                    {(f32)center_x, (f32)center_y });

                if (dist < radius)
                {
                    //u32 dst_color = color_v4f_to_u32(to_premultiplied(color_u32_to_v4f(raster_buffer[index]).rgb, 1.0f));
                    u32 dst_color = raster_buffer[index];
                    v4f blended = blend_v4f(color_u32_to_v4f(dst_color), src_color);
                    u32 color = color_v4f_to_u32(blended);
                    raster_buffer[index] = color;
                }
            }
        }
    }
}

#if 0
static void draw_rectangle(u32* raster_buffer,
                           i32 raster_buffer_width, i32 raster_buffer_height,
                           i32 center_x, i32 center_y,
                           i32 rect_w, i32 rect_h,
                           v4f src_color)
{
    i32 left = max(center_x - rect_w, 0);
    i32 right = min(center_x + rect_w, raster_buffer_width);
    i32 top = max(center_y - rect_h, 0);
    i32 bottom = min(center_y + rect_h, raster_buffer_height);

    assert (right >= left);
    assert (bottom >= top);

    for ( i32 j = top; j < bottom; ++j )
    {
        for ( i32 i = left; i < right; ++i )
        {
            i32 index = j * raster_buffer_width + i;

            u32 dst_color = raster_buffer[index];
            v4f blended = blend_v4f(color_u32_to_v4f(dst_color), src_color);
            u32 color = color_v4f_to_u32(blended);
            raster_buffer[index] = color;
        }
    }
}
#endif


// 1-pixel margin
static void fill_rectangle_with_margin(u32* raster_buffer,
                                       i32 raster_buffer_width, i32 raster_buffer_height,
                                       i32 x, i32 y,
                                       i32 w, i32 h,
                                       v4f rect_color, v4f margin_color)
{
    i32 left = max(x, 0);
    i32 right = min(x + w, raster_buffer_width);
    i32 top = max(y, 0);
    i32 bottom = min(y + h, raster_buffer_height);

    if ( (right >= left)
         &&(bottom >= top))
    {
        for ( i32 j = top; j < bottom; ++j )
        {
            for ( i32 i = left; i < right; ++i )
            {
                i32 index = j * raster_buffer_width + i;

                u32 dst_color = raster_buffer[index];
                v4f src_color = (i == left || i == right - 1 ||
                                 j == top || j == bottom - 1) ? margin_color : rect_color;

                v4f blended = blend_v4f(color_u32_to_v4f(dst_color), src_color);
                u32 color = color_v4f_to_u32(blended);
                raster_buffer[index] = color;
            }
        }
    }
}

static void rectangle_margin(u32* raster_buffer,
                             i32 raster_buffer_width, i32 raster_buffer_height,
                             i32 x, i32 y,
                             i32 w, i32 h,
                             v4f margin_color, i32 margin_px)
{
    i32 left = max(x, 0);
    i32 right = min(x + w, raster_buffer_width);
    i32 top = max(y, 0);
    i32 bottom = min(y + h, raster_buffer_height);

#define BLEND_HERE() \
    i32 index = j * raster_buffer_width + i; \
    u32 dst_color = raster_buffer[index]; \
    v4f blended = blend_v4f(color_u32_to_v4f(dst_color), margin_color); \
    u32 color = color_v4f_to_u32(blended); \
    raster_buffer[index] = color; \


    for ( i32 j = top; j < bottom; ++j )
    {
        for ( i32 i = left; i <= left + margin_px; ++i )
        {
            BLEND_HERE();
        }
        for ( i32 i = right - margin_px; i <= right; ++i )
        {
            BLEND_HERE();
        }
    }

    for ( i32 i = left; i < right; ++i )
    {
        for ( i32 j = top; j <= top + margin_px; ++j)
        {
            BLEND_HERE();
        }
        for ( i32 j = bottom - margin_px; j <= bottom; ++j )
        {
            BLEND_HERE();
        }
    }

#undef BLEND_AT_INDEX
}

static b32 stroke_intersects_rect(Stroke* stroke, Rect rect)
{
    b32 intersects = rect_intersects_rect(stroke->bounding_rect, rect);
    if (intersects)
    { // First intersection with bounding rect. Then check with points.
        intersects = false;
        rect = rect_enlarge(rect, stroke->brush.radius);
        if (rect_is_valid(rect))
        {
            if (stroke->num_points == 1)
            {
                if ( is_inside_rect(rect, stroke->points[0]) )
                {
                    intersects = true;
                }
            }
            else
            {
                for (size_t point_i = 0; point_i < (size_t)stroke->num_points - 1; ++point_i)
                {
                    v2i a = stroke->points[point_i    ];
                    v2i b = stroke->points[point_i + 1];

                    b32 inside = !((a.x > rect.right && b.x >  rect.right) ||
                                   (a.x < rect.left && b.x <   rect.left) ||
                                   (a.y < rect.top && b.y <    rect.top) ||
                                   (a.y > rect.bottom && b.y > rect.bottom));

                    if (inside)
                    {
                        intersects = true;
                        break;
                    }
                }
            }
        }
        else
        {
            milton_log("Stroke intersection invalid!\n");
        }
    }
    return intersects;
}

static void rasterize_color_picker(ColorPicker* picker, Rect draw_rect)
{
    v4f background_color =
    {
        0.5f,
        0.5f,
        0.55f,
        0.4f,
    };
    background_color = to_premultiplied(background_color.rgb, background_color.a);

    // Render background color.
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 picker_i = (u32) (j - draw_rect.top) *( 2*picker->bounds_radius_px ) + (i - draw_rect.left);

            picker->pixels[picker_i] = color_v4f_to_u32(background_color);
        }
    }

    // Wheel
    for ( int j = draw_rect.top; j < draw_rect.bottom; ++j )
    {
        for ( int i = draw_rect.left; i < draw_rect.right; ++i )
        {
            u32 picker_i = (u32) (j - draw_rect.top) *( 2*picker->bounds_radius_px ) + (i - draw_rect.left);
            v2f point = {(f32)i, (f32)j};
            u32 dest_color = picker->pixels[picker_i];

            int samples = 0;
            {
                // Rotated 2x2 grid
                //  The u,v magic values are related to a 2d rotation by some angle,
                //  I should have commented this before forgetting
                //        ()--------()
                //       /         /
                //     /         /
                //   ()--------()
                f32 u = 0.223607f;
                f32 v = 0.670820f;

                samples += (int)picker_hits_wheel(picker, add2f(point, {-u, -v}));
                samples += (int)picker_hits_wheel(picker, add2f(point, {-v, u}));
                samples += (int)picker_hits_wheel(picker, add2f(point, {u, v}));
                samples += (int)picker_hits_wheel(picker, add2f(point, {v, u}));
            }

            if (samples > 0)
            {
                f32 angle = picker_wheel_get_angle(picker, point);
                f32 degree = radians_to_degrees(angle);
                v3f hsv = { degree, 1.0f, 1.0f };

                f32 alpha = samples / 4.0f;
                v4f rgba  = to_premultiplied(hsv_to_rgb(hsv), alpha);

                v4f d = color_u32_to_v4f(dest_color);

                v4f result = blend_v4f(d, rgba);
                u32 color = color_v4f_to_u32(result);
                picker->pixels[picker_i] = color;
            }
        }
    }

    // Triangle
    for ( int j = draw_rect.top; j < draw_rect.bottom; ++j )
    {
        for ( int i = draw_rect.left; i < draw_rect.right; ++i )
        {
            v2f point = { (f32)i, (f32)j };
            u32 picker_i = (u32) (j - draw_rect.top) *( 2*picker->bounds_radius_px ) + (i - draw_rect.left);
            u32 dest_color = picker->pixels[picker_i];
            // MSAA!!
            int samples = 0;
            {
                // Rotated 2x2 grid
                //  The u,v magic values are related to a 2d rotation by some angle,
                //  I should have commented this before forgetting
                //        ()--------()
                //       /         /
                //     /         /
                //   ()--------()
                f32 u = 0.223607f;
                f32 v = 0.670820f;

                samples += (int)is_inside_triangle(add2f(point, {-u, -v}), picker->data.a, picker->data.b, picker->data.c);
                samples += (int)is_inside_triangle(add2f(point, {-v, u}), picker->data.a, picker->data.b, picker->data.c);
                samples += (int)is_inside_triangle(add2f(point, {u, v}), picker->data.a, picker->data.b, picker->data.c);
                samples += (int)is_inside_triangle(add2f(point, {v, u}), picker->data.a, picker->data.b, picker->data.c);
            }

            if (samples > 0)
            {
                v3f hsv = picker_hsv_from_point(picker, point);

                f32 contrib = samples / 4.0f;

                v4f d = color_u32_to_v4f(dest_color);

                f32 alpha = contrib;
                v4f rgba  = to_premultiplied(hsv_to_rgb(hsv), alpha);

                v4f result = blend_v4f(d, rgba);

                picker->pixels[picker_i] = color_v4f_to_u32(result);
            }
        }
    }

    // Black ring around the chosen color.
    {
        i32 ring_radius = 5;
        i32 ring_girth = 1;

        v3f hsv = picker->data.hsv;

        v3f rgb = hsv_to_rgb(hsv);

        v4f color = v4f{
            1 - rgb.r,
            1 - rgb.g,
            1 - rgb.b,
            1,
        };

        if ((picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE))
        {
            ring_radius = 10;
            ring_girth = 2;
            color = v4f{0,0,0,1};
        }

        // Barycentric to cartesian
        f32 a = hsv.s;
        f32 b = 1 - hsv.v;
        f32 c = 1 - a - b;

        v2f point = add2f(add2f((scale2f(picker->data.c,a)), scale2f(picker->data.b,b)), scale2f(picker->data.a,c));

        // De-center
        point.x -= picker->center.x - picker->bounds_radius_px;
        point.y -= picker->center.y - picker->bounds_radius_px;

        i32 width = picker->bounds_radius_px * 2;
        i32 height = picker->bounds_radius_px * 2;

        draw_ring(picker->pixels, width, height,
                  (i32)point.x, (i32)point.y,
                  ring_radius, ring_girth,
                  color);
    }
}

static void fill_stroke_masks_for_worker(Layer* layer, Rect rect, i32 worker_id)
{
    while (layer)
    {
        if (!(layer->flags & LayerFlags_VISIBLE))
        {
            layer = layer->next;
            continue;
        }
        Stroke* strokes = layer->strokes.data;

        for (u64 stroke_i = 0;
             stroke_i < layer->strokes.count;
             ++stroke_i)
        {
            Stroke* stroke = strokes + stroke_i;
            stroke->visibility[worker_id] = stroke_intersects_rect(stroke, rect);
        }
        layer = layer->next;
    }
}


// Returns false if there were allocation errors
static b32 render_blockgroup(MiltonState* milton_state,
                             i32 worker_id,
                             Arena* blockgroup_arena,
                             Rect* blocks,
                             i32 block_start, i32 num_blocks,
                             u32* raster_buffer)
{
    b32 allocation_ok = true;

    Rect raster_blockgroup_rect = { 0 };
    Rect canvas_blockgroup_rect = { 0 };

    const i32 blocks_per_blockgroup = milton_state->blocks_per_blockgroup;
    // Clip and move to canvas space.
    // Derive blockgroup_rect
    for ( i32 block_i = 0; block_i < blocks_per_blockgroup; ++block_i )
    {
        if ( block_start + block_i >= num_blocks )
        {
            break;
        }
        blocks[block_start + block_i] = rect_clip_to_screen(blocks[block_start + block_i],
                                                            milton_state->view->screen_size);
        raster_blockgroup_rect = rect_union(raster_blockgroup_rect, blocks[block_start + block_i]);

        canvas_blockgroup_rect.top_left =
                raster_to_canvas(milton_state->view, raster_blockgroup_rect.top_left);
        canvas_blockgroup_rect.bot_right =
                raster_to_canvas(milton_state->view, raster_blockgroup_rect.bot_right);
    }

    // Our version of SDL defines SDL_atomic_t as typdedef struct { int value } SDL_atomic_t;
    // PROFILE_GRAPH_BEGIN
    //milton_state->graph_frame.start = perf_counter();
#if MILTON_ENABLE_PROFILING
    SDL_AtomicCAS((SDL_atomic_t*)&milton_state->graph_frame.start,
                  (int)milton_state->graph_frame.start,
                  (int)perf_counter());
#endif
    fill_stroke_masks_for_worker(milton_state->root_layer, canvas_blockgroup_rect, worker_id);
    // PROFILE_RASTER_PUSH
    //    milton_state->graph_frame.##name = perf_counter() - milton_state->graph_frame.start
#if MILTON_ENABLE_PROFILING
    SDL_AtomicCAS((SDL_atomic_t*)&milton_state->graph_frame.clipping,
                  (int)milton_state->graph_frame.clipping,
                  (int)(perf_counter() - milton_state->graph_frame.start));
#endif
    Arena render_arena = { 0 };
    if (allocation_ok)
    {
        render_arena = arena_spawn(blockgroup_arena, arena_available_space(blockgroup_arena));
    }

    for (int block_i = 0; block_i < blocks_per_blockgroup && allocation_ok; ++block_i)
    {
        if (block_start + block_i >= num_blocks)
        {
            break;
        }

#if MILTON_USE_ALL_RENDERERS == 0
#if MILTON_DEBUG
        if (SDL_HasSSE2() && !milton_state->DEBUG_sse2_switch)
        {
#else
        if (SDL_HasSSE2())
        {
#endif
            allocation_ok = rasterize_canvas_block_sse2(&render_arena,
                                                        worker_id,
                                                        milton_state->view,
                                                        milton_state->root_layer,
                                                        &milton_state->working_stroke,
                                                        raster_buffer,
                                                        blocks[block_start + block_i]);
        }
        else
        {
            allocation_ok = rasterize_canvas_block_slow(&render_arena,
                                                        worker_id,
                                                        milton_state->view,
                                                        milton_state->root_layer,
                                                        &milton_state->working_stroke,
                                                        raster_buffer,
                                                        blocks[block_start + block_i]);
        }
#else  // MILTON_USE_ALL_RENDERERS == 1
        // Use a sampling profiler to compare speeds
        allocation_ok = rasterize_canvas_block_sse2(&render_arena,
                                                    worker_id,
                                                    milton_state->view,
                                                    milton_state->root_layer,
                                                    &milton_state->working_stroke,
                                                    raster_buffer,
                                                    blocks[block_start + block_i]);
        allocation_ok = rasterize_canvas_block_slow(&render_arena,
                                                    worker_id,
                                                    milton_state->view,
                                                    milton_state->root_layer,
                                                    &milton_state->working_stroke,
                                                    raster_buffer,
                                                    blocks[block_start + block_i]);
#endif // MILTON_USE_ALL_RENDERERS
        arena_reset_noclear(&render_arena);
        //arena_reset(&render_arena);
    }
    return allocation_ok;
}

int  // Thread
renderer_worker_thread(void* data)
{
    WorkerParams* params = (WorkerParams*) data;
    MiltonState* milton_state = params->milton_state;
    i32 worker_id = params->worker_id;
    RenderStack* render_stack = milton_state->render_stack;

    for ( ;; )
    {
        int err = SDL_SemWait(render_stack->work_available);
        if ( err != 0 )
        {
            milton_fatal("Failure obtaining semaphore inside render worker");
        }
        BlockgroupRenderData blockgroup_data = { 0 };
        i32 index = -1;

        err = SDL_LockMutex(render_stack->mutex);
        if ( err != 0 )
        {
            milton_fatal("Failure locking render queue mutex");
        }
        index = --render_stack->index;
        assert (index >= 0);
        assert (index <  RENDER_STACK_SIZE);
        blockgroup_data = render_stack->blockgroup_render_data[index];
        SDL_UnlockMutex(render_stack->mutex);

        assert (index >= 0);

        b32 allocation_ok = render_blockgroup(milton_state,
                                              worker_id,
                                              &milton_state->render_worker_arenas[worker_id],
                                              render_stack->blocks,
                                              blockgroup_data.block_start, render_stack->num_blocks,
                                              render_stack->canvas_buffer);
        if ( !allocation_ok )
        {
            milton_state->flags |= MiltonStateFlags_WORKER_NEEDS_MEMORY;
        }

        arena_reset_noclear(&milton_state->render_worker_arenas[worker_id]);

        SDL_SemPost(render_stack->completed_semaphore);
    }
}

#if MILTON_MULTITHREADED
static void produce_render_work(MiltonState* milton_state,
                                BlockgroupRenderData blockgroup_render_data)
{
    RenderStack* render_stack = milton_state->render_stack;

    {
        i32 lock_err = SDL_LockMutex(render_stack->mutex);
        if ( !lock_err )
        {
            if ( render_stack->index < RENDER_STACK_SIZE )
            {
                render_stack->blockgroup_render_data[render_stack->index++] = blockgroup_render_data;
            }
            SDL_UnlockMutex(render_stack->mutex);
        }
    }

    SDL_SemPost(render_stack->work_available);
}
#endif

static void render_canvas(MiltonState* milton_state, Rect raster_limits)
{
    PROFILE_RASTER_BEGIN(render_canvas);

    Rect* blocks = NULL;
    i32 num_blocks = rect_split(&blocks, raster_limits, milton_state->block_width, milton_state->block_width);

    if ( num_blocks > 0 ) assert(blocks != NULL);

    RenderStack* render_stack = milton_state->render_stack;
    {
        render_stack->blocks = blocks;
        render_stack->num_blocks = (i32)num_blocks;
        render_stack->canvas_buffer = (u32*)milton_state->canvas_buffer;
    }

    const i32 blocks_per_blockgroup = milton_state->blocks_per_blockgroup;

    i32 blockgroup_acc = 0;
    for ( int block_i = 0; block_i < num_blocks; block_i += blocks_per_blockgroup )
    {
        if ( block_i >= num_blocks )
        {
            break;
        }
#if MILTON_MULTITHREADED
        BlockgroupRenderData data =
        {
            block_i,
        };

        produce_render_work(milton_state, data);
        blockgroup_acc += 1;
#else
        Arena blockgroup_arena = arena_push(milton_state->root_arena, (size_t)128 * 1024 * 1024);
        render_blockgroup(milton_state,
                          0,
                          &blockgroup_arena,
                          blocks,
                          block_i, num_blocks,
                          (u32*)milton_state->canvas_buffer);

        arena_pop(&blockgroup_arena);
#endif
    }

#if MILTON_MULTITHREADED
    // Wait for workers to finish.

    while ( blockgroup_acc )
    {
        i32 waited_err = SDL_SemWait(milton_state->render_stack->completed_semaphore);
        if (!waited_err)
        {
            --blockgroup_acc;
        }
        else { milton_die_gracefully ( "Semaphore wait error (rasterizer.c:render_canvas)" ); }
    }
#endif

    if (blocks)
    {
        mlt_free(blocks);
    }

    PROFILE_RASTER_PUSH(render_canvas);
}

// Call render_canvas with increasing quality until it is too slow.
static void render_canvas_iteratively(MiltonState* milton_state, Rect raster_limits)
{
    CanvasView* view = milton_state->view;
    i32 original_df = view->downsampling_factor;

    i32 time_ms = 0;
    view->downsampling_factor = 8;

    while (view->downsampling_factor > 0 && time_ms < 30)
    {
        i32 start_ms = (i32)SDL_GetTicks();
        render_canvas(milton_state, raster_limits);
        time_ms += SDL_GetTicks() - start_ms;
        view->downsampling_factor /= 2;
    }

    view->downsampling_factor = original_df;
}

static void render_picker(int render_flags, ColorPicker* picker, u32* buffer_pixels, CanvasView* view)
{
    Rect draw_rect = picker_get_bounds(picker);

    if ((render_flags & MiltonRenderFlags_UI_UPDATED))
    {
        rasterize_color_picker(picker, draw_rect);
    }

    // Blend picker pixels
    u32* to_blend = picker->pixels;
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            v4f picker_color = color_u32_to_v4f(*to_blend++);
            auto buf_color = color_u32_to_v4f(buffer_pixels[j * view->screen_size.w + i]);
            buffer_pixels[j * view->screen_size.w + i] = color_v4f_to_u32(blend_v4f(buf_color, picker_color));
        }
    }
}

// Commented out because it's not useless, but I don't think Milton will blit bitmaps
#if 0
static void blit_bitmap(u32* raster_buffer, i32 raster_buffer_width, i32 raster_buffer_height,
                        i32 x, i32 y, Bitmap* bitmap)
{

    if ( !bitmap->data )
    {
        return;
    }
    Rect limits = {0};
    limits.left   = max(x, 0);
    limits.top    = max(y, 0);
    limits.right  = min(raster_buffer_width, x + bitmap->width);
    limits.bottom = min(raster_buffer_height, y + bitmap->height);

    if (bitmap->num_components != 4)
    {
        assert (!"not implemented");
    }

    u32* src_data = (u32*)bitmap->data;
    for ( i32 j = limits.top; j < limits.bottom; ++j )
    {
        for ( i32 i = limits.left; i < limits.right; ++i )
        {
            i32 index = j * raster_buffer_width + i;
            raster_buffer[index] = *src_data++;
        }
    }

}
#endif

static void copy_canvas_to_raster_buffer(MiltonState* milton_state, Rect rect)
{
    u32* raster_ptr = (u32*)milton_state->raster_buffer;
    u32* canvas_ptr = (u32*)milton_state->canvas_buffer;
    for ( i32 j = rect.top; j <= rect.bottom; ++j )
    {
        for ( i32 i = rect.left; i <= rect.right; ++i )
        {
            i32 bufi = j*milton_state->view->screen_size.w + i;
            raster_ptr[bufi] = canvas_ptr[bufi];
        }
    }
}

static void render_gui(MiltonState* milton_state, Rect raster_limits, int/*MiltonRenderFlags*/ render_flags)
{
    const b32 gui_visible = milton_state->gui->visible;  // Some elements are not affected by this, like the hover outline
    b32 redraw = false;
    Rect picker_rect = get_bounds_for_picker_and_colors(&milton_state->gui->picker);
    Rect clipped = rect_intersect(picker_rect, raster_limits);
    if ( (clipped.left != clipped.right) && clipped.top != clipped.bottom )
    {
        redraw = true;
    }
    u32* raster_buffer = (u32*)milton_state->raster_buffer;
    MiltonGui* gui = milton_state->gui;
    if ( gui_visible && (redraw || (render_flags & MiltonRenderFlags_UI_UPDATED)))
    {
        render_picker(render_flags,
                      &milton_state->gui->picker,
                      raster_buffer,
                      milton_state->view);

        // Render history buttons for picker
        ColorButton* button = &milton_state->gui->picker.color_buttons;
        while(button)
        {
            if (button->rgba.a == 0)
            {
                break;
            }
            fill_rectangle_with_margin(raster_buffer,
                                       milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                                       button->x, button->y,
                                       button->w, button->h,
                                       button->rgba,
                                       // Black margin
                                       v4f{ 0, 0, 0, 1 });
            button = button->next;
        }

        // Draw an outlined circle for selected color.
        i32 circle_radius = 20;
        i32 picker_radius = gui->picker.bounds_radius_px;
        i32 ring_girth = 1;
        i32 center_shift = picker_radius - circle_radius - 2*ring_girth;
        i32 x = gui->picker.center.x + center_shift;
        i32 y = gui->picker.center.y - center_shift;
        draw_circle(raster_buffer,
                    milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                    x, y,
                    circle_radius,
                    color_rgb_to_rgba(hsv_to_rgb(gui->picker.data.hsv), 1.0f));
        draw_ring(raster_buffer,
                  milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                  x, y,
                  circle_radius, ring_girth,
                  v4f{0,0,0,1});
    }


    if (gui_visible)
    {  // Render button
        if ((render_flags & MiltonRenderFlags_BRUSH_PREVIEW))
        {
            assert (gui->preview_pos.x >= 0 && gui->preview_pos.y >= 0);
            const i32 radius = milton_get_brush_size(milton_state);

            v4f preview_color;
            preview_color.rgb = milton_state->view->background_color;
            preview_color.a = 1;
            if ( milton_state->current_mode == MiltonMode_PEN )
            {
                preview_color = to_premultiplied(hsv_to_rgb(gui->picker.data.hsv), milton_get_pen_alpha(milton_state));
            }
            draw_circle(raster_buffer,
                        milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                        gui->preview_pos.x, gui->preview_pos.y,
                        radius,
                        preview_color);
            draw_ring(raster_buffer,
                      milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                      gui->preview_pos.x, gui->preview_pos.y,
                      radius, 2,
                      v4f{0,0,0,1});
            gui->preview_pos_prev = gui->preview_pos;
            gui->preview_pos = v2i{ -1, -1 };
        }
    }

    // If the explorer is active, render regardless of UI being visible

    if ( milton_state->current_mode == MiltonMode_EXPORTING  )
    {
        Exporter* exporter = &gui->exporter;
        if ( exporter->state == ExporterState_GROWING_RECT || exporter->state == ExporterState_SELECTED )
        {
            i32 x = min(exporter->pivot.x, exporter->needle.x);
            i32 y = min(exporter->pivot.y, exporter->needle.y);
            i32 w = abs(exporter->pivot.x - exporter->needle.x);
            i32 h = abs(exporter->pivot.y - exporter->needle.y);

            rectangle_margin(raster_buffer,
                             milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                             x,y,w,h,
                             v4f{ 0, 0, 0, 1 }, 1);
        }
    }

    if ((render_flags & MiltonRenderFlags_BRUSH_HOVER))
    {
        float outline_alpha = 1.0f;
        float gray = 0.25f;
        const i32 radius = milton_get_brush_size(milton_state);

        // Draw brush outline if...
        if (milton_state->current_mode == MiltonMode_ERASER ||
            radius > MILTON_HIDE_BRUSH_OVERLAY_AT_THIS_SIZE ||
            (i32)SDL_GetTicks() - milton_state->hover_flash_ms < HOVER_FLASH_THRESHOLD_MS)
        {
            draw_ring(raster_buffer,
                      milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                      milton_state->hover_point.x, milton_state->hover_point.y,
                      radius, 1,
                      v4f{gray,gray,gray, outline_alpha});
            draw_ring(raster_buffer,
                      milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                      milton_state->hover_point.x, milton_state->hover_point.y,
                      radius+1, 1,
                      v4f{1-gray,1-gray,1-gray, outline_alpha});
        }
    }
}

void milton_render(MiltonState* milton_state, int render_flags, v2i pan_delta)
{
    PROFILE_GRAPH_BEGIN(raster);
    b32 canvas_modified = false;    // Avoid needless work when program is idle by keeping track
                                    // of when everything needs to be redrawn. The gui needs to know
                                    // about the canvas.
    // `raster_limits` is the part of the screen (in pixels) that should be updated
    // with what's on the canvas.
    Rect raster_limits = { 0 };

    //  - If the screen is panning, copy the part of the buffer we can reuse.
    //  - For whatever reason, we might be asked a full redraw.
    //  - If we don't get a full redraw, then figure out what to draw based on the
    //      current working stroke.

#if MILTON_ENABLE_RASTER_PROFILING
    set_flag(render_flags, MiltonRenderFlags_FULL_REDRAW);
#endif

    if ((render_flags & MiltonRenderFlags_PAN_COPY))
    {
        canvas_modified = true;
        CanvasView* view = milton_state->view;

        // Copy the canvas buffer to the raster buffer. We will use the raster
        // buffer as temporary storage. It will be rewritten later.
        memcpy(milton_state->raster_buffer, milton_state->canvas_buffer,
               (size_t)milton_state->bytes_per_pixel * view->screen_size.w * view->screen_size.h);


        // Dimensions of rectangle
        v2i size =
        {
            view->screen_size.w - abs(pan_delta.x),
            view->screen_size.h - abs(pan_delta.y),
        };

        // Call render canvas on two new rectangles representing the 'dirty' area.
        //
        //  +-----------.---+
        //  |           .   |
        //  | memcpy    . V |
        //  |............   |  V: Vertical rectangle
        //  |    H      .   |  H: Horizontal rectangle
        //  +---------------+


        i32 pad = milton_state->block_width;

        Rect vertical = {0};
        Rect horizontal = {0};

        horizontal.left  = 0;
        horizontal.right = view->screen_size.w;

        vertical.top = 0;
        vertical.bottom = view->screen_size.h;

        if ( pan_delta.x > 0 )
        {
            vertical.left  = 0;
            vertical.right = max(pan_delta.x, pad);
        }
        else
        {
            vertical.left  = view->screen_size.w + min(pan_delta.x, -pad);
            vertical.right = view->screen_size.w;
        }

        if ( pan_delta.y > 0 )
        {
            horizontal.top    = 0;
            horizontal.bottom = max(pan_delta.y, pad);
        }
        else //if ( pan_delta.y < 0 )
        {
            horizontal.top    = view->screen_size.h + min(pan_delta.y, -pad);
            horizontal.bottom = view->screen_size.h;
        }

        // Extend the rects to cover at least one block
        {
            int vw = vertical.right - vertical.left;
            int vh = vertical.bottom - vertical.top;
            int hw = horizontal.right - horizontal.left;
            int hh = horizontal.bottom - horizontal.top;
            if (vw > 0 && vw < pad)
            {
                vw = pad;
            }
            if (vh > 0 && vh < pad)
            {
                vw = pad;
            }
            if (hw > 0 && vw < pad)
            {
                vw = pad;
            }
            if (hh > 0 && hh < pad)
            {
                vw = pad;
            }
        }


        render_canvas_iteratively(milton_state, horizontal);
        render_canvas_iteratively(milton_state, vertical);

        v2i dst = {0};  // Starting point to copy canvas buffer
        if ( pan_delta.x > 0 )
        {
            dst.x = pan_delta.x;
        }
        if ( pan_delta.y > 0 )
        {
            dst.y = pan_delta.y;
        }

        v2i src = {0};  // Starting point of source canvas buffer.
        if ( pan_delta.x < 0 )
        {
            src.x = -pan_delta.x;
        }
        if ( pan_delta.y < 0 )
        {
            src.y = -pan_delta.y;
        }

        u32* pixels_src = ((u32*)milton_state->raster_buffer) + (src.y*view->screen_size.w + src.x);
        u32* pixels_dst = ((u32*)milton_state->canvas_buffer) + (dst.y*view->screen_size.w + dst.x);

        for ( int j = 0; j < size.h; ++j )
        {
            for ( int i = 0; i < size.w; ++i )
            {
                *pixels_dst++ = *pixels_src++;
            }
            pixels_dst += view->screen_size.w - size.w;
            pixels_src += view->screen_size.w - size.w;
        }

    }

    if ((render_flags & MiltonRenderFlags_FULL_REDRAW))
    {
        raster_limits.left = 0;
        raster_limits.right = milton_state->view->screen_size.w;
        raster_limits.top = 0;
        raster_limits.bottom = milton_state->view->screen_size.h;
        raster_limits = rect_stretch(raster_limits, milton_state->block_width);
    }
    else if (milton_state->working_stroke.num_points > 1)
    {
        Stroke* stroke = &milton_state->working_stroke;

        raster_limits = bounding_box_for_last_n_points(stroke, 20);
        raster_limits = canvas_rect_to_raster_rect(milton_state->view, raster_limits);
        raster_limits = rect_stretch(raster_limits, milton_state->block_width);
        raster_limits = rect_stretch(raster_limits, milton_state->block_width);
        raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);

    }
    else if (milton_state->working_stroke.num_points == 1)
    {
        Stroke* stroke = &milton_state->working_stroke;
        v2i point = canvas_to_raster(milton_state->view, stroke->points[0]);
        i32 raster_radius = stroke->brush.radius / milton_state->view->scale;
        raster_radius = max(raster_radius, milton_state->block_width);
        raster_limits.left   = -raster_radius + point.x;
        raster_limits.right  = raster_radius  + point.x;
        raster_limits.top    = -raster_radius + point.y;
        raster_limits.bottom = raster_radius  + point.y;
        raster_limits = rect_stretch(raster_limits, milton_state->block_width);
        raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);
    }
    else if ((render_flags & MiltonRenderFlags_FINISHED_STROKE))
    {
        Stroke* strokes = milton_state->working_layer->strokes.data;
        u64 index = milton_state->working_layer->strokes.count - 1;
        Rect canvas_rect = bounding_box_for_last_n_points(&strokes[index], 4);
        raster_limits = canvas_rect_to_raster_rect(milton_state->view, canvas_rect);
        raster_limits = rect_stretch(raster_limits, milton_state->block_width);
        raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);
    }

    if (rect_is_valid(raster_limits))
    {
        if (rect_area(raster_limits) != 0)
        {
            canvas_modified = true;
            if ((render_flags & MiltonRenderFlags_DRAW_ITERATIVELY))
            {
                render_canvas_iteratively(milton_state, raster_limits);
            }
            else
            {
                render_canvas(milton_state, raster_limits);
            }
        }
    }
    else
    {
        milton_log("WARNING: Tried to render with invalid rect: (l r t b): %d %d %d %d\n",
                   raster_limits.left,
                   raster_limits.right,
                   raster_limits.top,
                   raster_limits.bottom);
    }

    MiltonGui* gui = milton_state->gui;

    if ((render_flags & MiltonRenderFlags_UI_UPDATED) ||
        (render_flags & MiltonRenderFlags_BRUSH_HOVER) ||
        canvas_modified)
    {  // Prepare rect, copy buffer, and call render_gui
        static v2i static_hp = {-1};
        v2i hp = milton_state->hover_point;
        b32 hovering = (render_flags & MiltonRenderFlags_BRUSH_CHANGE) || (hp.x != static_hp.x || hp.y != static_hp.y);
        static_hp = hp;

        b32 should_copy = hovering || canvas_modified ||
                (render_flags & MiltonRenderFlags_UI_UPDATED) || (render_flags & MiltonRenderFlags_FULL_REDRAW);

        if (should_copy)
        {
            Rect copy_rect = {0};
            copy_rect.left = 0;
            copy_rect.right = milton_state->view->screen_size.w;
            copy_rect.top = 0;
            copy_rect.bottom = milton_state->view->screen_size.h;
            copy_canvas_to_raster_buffer(milton_state, copy_rect);
        }

        raster_limits = {};

        // Add picker to render rect
        if ((should_copy || (render_flags & MiltonRenderFlags_PAN_COPY)))
        {
            raster_limits = rect_union(raster_limits, get_bounds_for_picker_and_colors(&gui->picker));
        }

        if ((render_flags & MiltonRenderFlags_BRUSH_HOVER) && hovering)
        {
            Rect hr = {0};

            int pad = milton_state->block_width / 2;

            hr.left   = hp.x - pad;
            hr.right  = hp.x + pad;
            hr.top    = hp.y - pad;
            hr.bottom = hp.y + pad;

            raster_limits = rect_union(raster_limits, hr);
        }

        render_gui(milton_state, raster_limits, render_flags);
    }
    PROFILE_GRAPH_PUSH(raster);  // Raster includes rasterizing the GUI color picker.
}

void milton_render_to_buffer(MiltonState* milton_state, u8* buffer,
                             i32 x, i32 y,
                             i32 w, i32 h,
                             int scale)
{
    u8* saved_buffer = milton_state->canvas_buffer;
    CanvasView saved_view = *milton_state->view;

    // Do basically the same thing as milton_resize, without allocating.

    v2i center = divide2i(milton_state->view->screen_size, 2);
    v2i pan_delta = sub2i(center, v2i{x + (w/2), y + (h/2)}) ;

    milton_state->view->pan_vector = add2i(milton_state->view->pan_vector, scale2i(pan_delta, milton_state->view->scale));

    i32 buf_w = w * scale;
    i32 buf_h = h * scale;

    milton_state->canvas_buffer = buffer;
    milton_state->view->screen_size = v2i{ buf_w, buf_h };
    milton_state->view->screen_center = divide2i(milton_state->view->screen_size, 2);
    if ( scale > 1 )
    {
        milton_state->view->scale = (i32)ceill(((f32)milton_state->view->scale / (f32)scale));
    }

    Rect raster_limits;
    raster_limits.left   = 0;
    raster_limits.top    = 0;
    raster_limits.right  = buf_w;
    raster_limits.bottom = buf_h;

    // render_canvas will set worker_needs_memory to true if it fails to render.
    while ( render_canvas(milton_state, raster_limits), (milton_state->flags & MiltonStateFlags_WORKER_NEEDS_MEMORY) )
    {
        milton_expand_render_memory(milton_state);
    }

    // Unset
    milton_state->canvas_buffer = saved_buffer;
    *milton_state->view = saved_view;
}
