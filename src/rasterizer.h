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


typedef struct ClippedStroke_s ClippedStroke;
struct ClippedStroke_s
{
    b32     fills_block;
    Brush   brush;
    i32     num_points;
    v2i*    points;

    ClippedStroke* next;
};

func ClippedStroke* stroke_clip_to_rect(Arena* render_arena, Stroke* stroke, Rect canvas_rect)
{
    ClippedStroke* clipped_stroke = arena_alloc_elem(render_arena, ClippedStroke);

    if (clipped_stroke)
    {
        clipped_stroke->brush = stroke->brush;
        clipped_stroke->num_points = 0;
        clipped_stroke->points = arena_alloc_array(render_arena, stroke->num_points * 2, v2i);
    }
    else
    {
        return NULL;
    }
    if (!clipped_stroke->points)
    {
        return NULL;
    }

    if (stroke->num_points == 1)
    {
        if (is_inside_rect(canvas_rect, stroke->points[0]))
        {
            clipped_stroke->points[clipped_stroke->num_points++] = stroke->points[0];
        }
    }
    else
    {
        i32 num_points = stroke->num_points;
        for (i32 point_i = 0; point_i < num_points - 1; ++point_i)
        {
            v2i a = stroke->points[point_i];
            v2i b = stroke->points[point_i + 1];

            // Very conservative...
            b32 inside = !((a.x > canvas_rect.right && b.x > canvas_rect.right) ||
                           (a.x < canvas_rect.left && b.x < canvas_rect.left) ||
                           (a.y < canvas_rect.top && b.y < canvas_rect.top) ||
                           (a.y > canvas_rect.bottom && b.y > canvas_rect.bottom));

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

func v2i closest_point_in_segment(v2i a, v2i b,
                                    v2f ab, f32 ab_magnitude_squared,
                                    v2i canvas_point)
{
    v2i point;
    f32 mag_ab = sqrtf(ab_magnitude_squared);
    f32 d_x = ab.x / mag_ab;
    f32 d_y = ab.y / mag_ab;
    f32 ax_x = (f32)(canvas_point.x - a.x);
    f32 ax_y = (f32)(canvas_point.y - a.y);
    f32 disc = d_x * ax_x + d_y * ax_y;
    if (disc >= 0 && disc <= mag_ab)
    {
        point = (v2i)
        {
            (i32)(a.x + disc * d_x), (i32)(a.y + disc * d_y),
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
func b32 is_rect_filled_by_stroke(Rect rect, v2i reference_point,
                                    v2i* points, i32 num_points,
                                    Brush brush, CanvasView* view)
{
    // Perf note: With the current use, this is actually going to be zero.
    // Maybe turn this into an assert and avoid extra computations?
    v2i rect_center =
    {
        ((rect.left + rect.right) / 2) - reference_point.x,
        ((rect.top + rect.bottom) / 2) - reference_point.y,
    };

    if (num_points >= 2)
    {
        for (i32 point_i = 0; point_i < num_points; point_i += 2)
        {
            v2i a = points[point_i];
            v2i b = points[point_i + 1];

            a = sub_v2i(points[point_i], reference_point);
            b = sub_v2i(points[point_i + 1], reference_point);

            // Get closest point
            v2f ab = {(f32)(b.x - a.x), (f32)(b.y - a.y)};
            f32 mag_ab2 = ab.x * ab.x + ab.y * ab.y;
            v2i p  = closest_point_in_segment( a, b, ab, mag_ab2, rect_center);

            // Back to global coordinates
            p = add_v2i(p, reference_point);

            // Half width of a rectangle contained by brush at point p.
            i32 rad = (i32)(brush.radius * 0.707106781f);  // cos(pi/4)
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
        i32 rad = (i32)(brush.radius * 0.707106781f);  // cos(pi/4)
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

func v4f blend_v4f(v4f dst, v4f src)
{
    f32 alpha = 1 - ((1 - src.a) * (1 - dst.a));

    //f32 alpha = src.a + dst.a - (src.a * dst.a);
    v4f result =
    {
        src.r + dst.r * (1 - src.a),
        src.g + dst.g * (1 - src.a),
        src.b + dst.b * (1 - src.a),
        alpha
    };

    return result;
}

// returns false if allocation failed
func b32 rasterize_canvas_block(Arena* render_arena,
                                  CanvasView* view,
                                  ColorManagement cm,
                                  Stroke* strokes,
                                  b32* stroke_masks,
                                  i32 num_strokes,
                                  Stroke* working_stroke,
                                  u32* pixels,
                                  Rect raster_limits)
{
    b32 allocation_ok = true;
    //u64 pre_ccount_begin = __rdtsc();
    Rect canvas_limits;
    {
        canvas_limits.top_left  = raster_to_canvas(view, raster_limits.top_left);
        canvas_limits.bot_right = raster_to_canvas(view, raster_limits.bot_right);
    }

    if (canvas_limits.left   < -view->canvas_radius_limit ||
        canvas_limits.right  > view->canvas_radius_limit  ||
        canvas_limits.top    < -view->canvas_radius_limit ||
        canvas_limits.bottom > view->canvas_radius_limit
        )
    {
        for (int j = raster_limits.top; j < raster_limits.bottom; j++)
        {
            for (int i = raster_limits.left; i < raster_limits.right; i++)
            {
                pixels[j * view->screen_size.w + i] = 0xffff00ff;
            }
        }
        return allocation_ok;
    }
    ClippedStroke* stroke_list = NULL;

    v2i reference_point =
// Leaving this toggle-able for a quick way to show the cool precision error.
#if 1
    {
        (canvas_limits.left + canvas_limits.right) / 2,
        (canvas_limits.top + canvas_limits.bottom) / 2,
    };
#else
    {0};
#endif

    // Go forwards so we can do early reject with premultiplied alpha!
    for (i32 stroke_i = 0; stroke_i <= num_strokes; ++stroke_i)
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
        // ALlocation failed.
        // Handle this gracefully; this will cause more memory for render workers.
        if (!clipped_stroke)
        {
            allocation_ok = false;
            break;
        }

        if (clipped_stroke->num_points)
        {
            ClippedStroke* list_head = clipped_stroke;
            list_head->next = stroke_list;
            if (is_rect_filled_by_stroke(
                        canvas_limits, reference_point,
                        clipped_stroke->points, clipped_stroke->num_points, clipped_stroke->brush,
                        view))
            {
                list_head->fills_block = true;
            }
            stroke_list = list_head;
        }
    }

    // The ninetales factor, named after Roberto Lapuente and Ruben Bañuelos
    // for staying with me on twitch while solving this problem, is a floating
    // point precision hack for really close.
    // Zoom levels, to make things larger
    i32 ninetales = (view->scale <= 8) ? 4 : 1;

    {
        reference_point.x *= ninetales;
        reference_point.y *= ninetales;
    }

    // Set our `stroke_list` to end at the first opaque stroke that fills
    // this block.
    ClippedStroke* list_iter = stroke_list;
    while (list_iter)
    {
        if (list_iter->fills_block && list_iter->brush.color.a == 1.0f)
        {
            list_iter->next = NULL;
            break;
        }
        list_iter = list_iter->next;
    }

    i32 pixel_jump = view->downsampling_factor;  // Different names for the same thing.

    // This is the distance between two adjacent canvas pixels. Derived to
    // avoid expensive raster_to_canvas computations in the inner loop
    i32 canvas_jump = pixel_jump * view->scale * ninetales;

    // i and j are the canvas point
    i32 i = (((raster_limits.left - view->screen_center.x) *
              view->scale) - view->pan_vector.x) * ninetales - reference_point.x;
    i32 j = (((raster_limits.top - view->screen_center.y) *
              view->scale) - view->pan_vector.y) * ninetales - reference_point.y;

    for (i32 pixel_j = raster_limits.top; pixel_j < raster_limits.bottom; pixel_j += pixel_jump)
    {
        i = (((raster_limits.left - view->screen_center.x) *
                view->scale) - view->pan_vector.x) * ninetales - reference_point.x;
        for (i32 pixel_i = raster_limits.left; pixel_i < raster_limits.right; pixel_i += pixel_jump)
        {
            // Clear color
            v4f background_color = { 1, 1, 1, 1 };

            // Cumulative blending
            v4f acc_color = { 0 };

            ClippedStroke* list_iter = stroke_list;

            while(list_iter)
            {
                ClippedStroke* clipped_stroke      = list_iter;
                list_iter = list_iter->next;

                assert (clipped_stroke);

                assert (clipped_stroke->num_points % 2 == 0 || clipped_stroke->num_points <= 1);

                // Fast path.
                if (clipped_stroke->fills_block)
                {
#if 0 // Visualize it with black
                    v4f dst = {0,0,0,1};
#else
                    v4f dst = clipped_stroke->brush.color;
#endif
                    acc_color = blend_v4f(dst, acc_color);
                }
                else
                // Slow path. There are pixels not inside.
                {
                    v2i* points = clipped_stroke->points;

                    i32 batch_size = 0;  // Up to 4. How many points could we load from the stroke.

                    //v2f min_points[4] = {0};
                    f32 min_dist = FLT_MAX;
                    f32 dx = 0;
                    f32 dy = 0;

                    if (clipped_stroke->num_points == 1)
                    {
                        v2i first_point = points[0];
                        {
                            first_point.x *= ninetales;
                            first_point.y *= ninetales;
                        }
                        //min_points[0] = v2i_to_v2f(sub_v2i(first_point, reference_point));
                        v2i min_point = sub_v2i(first_point, reference_point);
                        dx = (f32) (i - min_point.x);
                        dy = (f32) (j - min_point.y);
                        min_dist = dx * dx + dy * dy;
                    }
                    else
                    {
                        // Find closest point.
#define USE_SSE 1
#if !USE_SSE
                        batch_size = 1;
                        for (int point_i = 0; point_i < clipped_stroke->num_points-1; point_i += 2)
                        {
                            v2i a = points[point_i];
                            v2i b = points[point_i + 1];
                            {
                                a.x *= ninetales;
                                a.y *= ninetales;
                                b.x *= ninetales;
                                b.y *= ninetales;
                            }
                            a = sub_v2i(a, reference_point);
                            b = sub_v2i(b, reference_point);

                            v2f ab = {(f32)(b.x - a.x), (f32)(b.y - a.y)};
                            f32 mag_ab2 = ab.x * ab.x + ab.y * ab.y;
                            if (mag_ab2 > 0)
                            {

                                v2f point = v2i_to_v2f(closest_point_in_segment(a, b,
                                                                                ab, mag_ab2,
                                                                                (v2i){i,j}));

                                f32 test_dx = (f32) (i - point.x);
                                f32 test_dy = (f32) (j - point.y);
                                f32 dist = test_dx * test_dx + test_dy * test_dy;
                                if (dist < min_dist)
                                {
                                    min_dist = dist;
                                    dx = test_dx;
                                    dy = test_dy;
                                }
                            }
                        }
#else
//#define SSE_M(wide, i) ((f32 *)&(wide) + i)
                        __m128 test_dx = _mm_set_ps1(0.0f);
                        __m128 test_dy = _mm_set_ps1(0.0f);
                        __m128 ab_x    = _mm_set_ps1(0.0f);
                        __m128 ab_y    = _mm_set_ps1(0.0f);
                        __m128 mag_ab  = _mm_set_ps1(0.0f);
                        __m128 d_x     = _mm_set_ps1(0.0f);
                        __m128 d_y     = _mm_set_ps1(0.0f);
                        __m128 ax_x    = _mm_set_ps1(0.0f);
                        __m128 ax_y    = _mm_set_ps1(0.0f);
                        __m128 disc    = _mm_set_ps1(0.0f);

                        for (int point_i = 0; point_i < clipped_stroke->num_points-1; point_i += 8)

                        {
                            f32 axs[4] = { 0 };
                            f32 ays[4] = { 0 };
                            f32 bxs[4] = { 0 };
                            f32 bys[4] = { 0 };
                            batch_size = 0;

                            for (i32 i = 0; i < 4; i++)
                            {
                                i32 index = point_i + 2*i;
                                if (index + 1 >= clipped_stroke->num_points)
                                {
                                    break;
                                }
                                // The point of reference point is to do the subtraction with
                                // integer arithmetic
                                axs[i] = (f32)(points[index    ].x * ninetales - reference_point.x);
                                ays[i] = (f32)(points[index    ].y * ninetales - reference_point.y);
                                bxs[i] = (f32)(points[index + 1].x * ninetales - reference_point.x);
                                bys[i] = (f32)(points[index + 1].y * ninetales - reference_point.y);
                                batch_size++;
                            }


                            __m128 a_x = _mm_set_ps((axs[3]),
                                                    (axs[2]),
                                                    (axs[1]),
                                                    (axs[0]));

                            __m128 b_x = _mm_set_ps((bxs[3]),
                                                    (bxs[2]),
                                                    (bxs[1]),
                                                    (bxs[0]));

                            __m128 a_y = _mm_set_ps((ays[3]),
                                                    (ays[2]),
                                                    (ays[1]),
                                                    (ays[0]));

                            __m128 b_y = _mm_set_ps((bys[3]),
                                                    (bys[2]),
                                                    (bys[1]),
                                                    (bys[0]));

                            ab_x = _mm_sub_ps(b_x, a_x);
                            ab_y = _mm_sub_ps(b_y, a_y);

                            mag_ab = _mm_add_ps(_mm_mul_ps(ab_x, ab_x),
                                                _mm_mul_ps(ab_y, ab_y));
                            // It's ok to fail the sqrt.
                            // sqrt slow. rsqrt fast
                            mag_ab = _mm_mul_ps(mag_ab, _mm_rsqrt_ps(mag_ab));

                            d_x = _mm_div_ps(ab_x, mag_ab);
                            d_y = _mm_div_ps(ab_y, mag_ab);

                            __m128 canvas_point_x4 = _mm_set_ps1((f32)i);
                            __m128 canvas_point_y4 = _mm_set_ps1((f32)j);

                            ax_x = _mm_sub_ps(canvas_point_x4, a_x);
                            ax_y = _mm_sub_ps(canvas_point_y4, a_y);

                            disc = _mm_add_ps(_mm_mul_ps(d_x, ax_x),
                                              _mm_mul_ps(d_y, ax_y));

                            // Clamp discriminant so that point lies in [a, b]
                            {
                                __m128 low  = _mm_set_ps1(0);
                                __m128 high = mag_ab;
                                disc = _mm_min_ps(_mm_max_ps(low, disc), high);
                            }

                            // (axs[i] + disc_i * (d_x[i],
                            __m128 point_x_4 = _mm_add_ps(a_x, _mm_mul_ps(disc, d_x));
                            __m128 point_y_4 = _mm_add_ps(a_y, _mm_mul_ps(disc, d_y));


                            test_dx = _mm_sub_ps(canvas_point_x4, point_x_4);
                            test_dy = _mm_sub_ps(canvas_point_y4, point_y_4);

                            __m128 dist4 = _mm_add_ps(_mm_mul_ps(test_dx, test_dx),
                                                      _mm_mul_ps(test_dy, test_dy));

                            f32 dists[4];
                            f32 tests_dx[4];
                            f32 tests_dy[4];
                            _mm_store_ps(dists, dist4);
                            _mm_store_ps(tests_dx, test_dx);
                            _mm_store_ps(tests_dy, test_dy);

                            for (i32 i = 0; i < batch_size; ++i)
                            {
                                f32 dist = dists[i];
                                if (dist < min_dist)
                                {
                                    min_dist = dist;
                                    dx = tests_dx[i];
                                    dy = tests_dy[i];
                                }
                            }
                        }
#endif
                    }

                    if (min_dist < FLT_MAX)
                    {
                        //u64 kk_ccount_begin = __rdtsc();
                        int samples = 0;
                        {
                            f32 f3 = (0.75f * view->scale) * pixel_jump * ninetales;
                            f32 f1 = (0.25f * view->scale) * pixel_jump * ninetales;
                            u32 radius = clipped_stroke->brush.radius * ninetales;
#define USE_SAMPLING_SSE 1
#if USE_SSE && !USE_SAMPLING_SSE
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
                            if (radius >= (1 << 16))
                            {
                                samples += (sqrtf(fdists[ 0]) < radius);
                                samples += (sqrtf(fdists[ 1]) < radius);
                                samples += (sqrtf(fdists[ 2]) < radius);
                                samples += (sqrtf(fdists[ 3]) < radius);
                                samples += (sqrtf(fdists[ 4]) < radius);
                                samples += (sqrtf(fdists[ 5]) < radius);
                                samples += (sqrtf(fdists[ 6]) < radius);
                                samples += (sqrtf(fdists[ 7]) < radius);
                                samples += (sqrtf(fdists[ 8]) < radius);
                                samples += (sqrtf(fdists[ 9]) < radius);
                                samples += (sqrtf(fdists[10]) < radius);
                                samples += (sqrtf(fdists[11]) < radius);
                                samples += (sqrtf(fdists[12]) < radius);
                                samples += (sqrtf(fdists[13]) < radius);
                                samples += (sqrtf(fdists[14]) < radius);
                                samples += (sqrtf(fdists[15]) < radius);
                            }
                            else
                            {
                                u32 sq_radius = radius * radius;

                                samples += (fdists[ 0] < sq_radius);
                                samples += (fdists[ 1] < sq_radius);
                                samples += (fdists[ 2] < sq_radius);
                                samples += (fdists[ 3] < sq_radius);
                                samples += (fdists[ 4] < sq_radius);
                                samples += (fdists[ 5] < sq_radius);
                                samples += (fdists[ 6] < sq_radius);
                                samples += (fdists[ 7] < sq_radius);
                                samples += (fdists[ 8] < sq_radius);
                                samples += (fdists[ 9] < sq_radius);
                                samples += (fdists[10] < sq_radius);
                                samples += (fdists[11] < sq_radius);
                                samples += (fdists[12] < sq_radius);
                                samples += (fdists[13] < sq_radius);
                                samples += (fdists[14] < sq_radius);
                                samples += (fdists[15] < sq_radius);
                            }
                            if (samples > 12)
                            {
                                int asdf =1;
                            }
#else
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
                            assert (radius > 0);
                            assert (radius < sqrtf((FLT_MAX)));

                            __m128 radius4 = _mm_set_ps1((f32)clipped_stroke->brush.radius *
                                                         ninetales);

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
#endif
                        }

                        // If the stroke contributes to the pixel, do compositing.
                        if (samples > 0)
                        {
                            // Do blending
                            // ---------------

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

                        /* g_kk_ccount += __rdtsc() - kk_ccount_begin; */
                        /* g_kk_calls++; */
                    }
                }
                if (acc_color.a > 0.9999)
                {
                    break;
                }
            }

            // Blend onto the background whatever is accumulated.
            acc_color = blend_v4f(background_color, acc_color);

            // Brushes are stored and operated in linear space, move to srgb
            // before blitting
            acc_color.rgb = linear_to_sRGB(acc_color.rgb);
            // From [0, 1] to [0, 255]
            u32 pixel = color_v4f_to_u32(cm, acc_color);

            // TODO: Bilinear sampling could be nice here
            for (i32 jj = pixel_j; jj < pixel_j + pixel_jump; ++jj)
            {
                for (i32 ii = pixel_i; ii < pixel_i + pixel_jump; ++ii)
                {
                    pixels[jj * view->screen_size.w + ii] = pixel;
                }
            }
            i += canvas_jump;
        }
        j += canvas_jump;
    }
    return allocation_ok;
}

