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
    b32             fills_block;

    // This stroke is clipped to a particular block and its pointers only live
    // as long as the render work lasts.
    Stroke          stroke;

    ClippedStroke*  next;
};

func ClippedStroke* stroke_clip_to_rect(Arena* render_arena, Stroke* in_stroke, Rect canvas_rect)
{
    ClippedStroke* clipped_stroke = arena_alloc_elem(render_arena, ClippedStroke);
    if (!clipped_stroke)
    {
        return NULL;
    }

    clipped_stroke->fills_block = false;
    clipped_stroke->next = NULL;
    clipped_stroke->stroke = *in_stroke;  // Copy everything except:

    // ... now substitute the point data with an array of our own.
    //
    // TODO: To save memory, it would make sense to use a different data
    // structure here.  A deque would have the advantages of a stretchy
    // array without asking too much from our humble arena.
    if (in_stroke->num_points > 0)
    {
        clipped_stroke->stroke.num_points = 0;
        clipped_stroke->stroke.points = arena_alloc_array(render_arena,
                                                          in_stroke->num_points, v2i);
        clipped_stroke->stroke.metadata = arena_alloc_array(render_arena,
                                                            in_stroke->num_points, PointMetadata);
    }
    else
    {
        return clipped_stroke;
    }


    if (!clipped_stroke->stroke.points || !clipped_stroke->stroke.metadata)
    {
        // We need more memory. Return gracefully
        return NULL;
    }
    if (in_stroke->num_points == 1)
    {
        if (is_inside_rect(canvas_rect, in_stroke->points[0]))
        {
            Stroke* stroke = &clipped_stroke->stroke;
            stroke->points[stroke->num_points++] = in_stroke->points[0];
        }
    }
    else
    {
        i32 num_points = in_stroke->num_points;
        b32 added_previous_segment = false;
        for (i32 point_i = 0; point_i < num_points - 1; ++point_i)
        {
            v2i a = in_stroke->points[point_i];
            v2i b = in_stroke->points[point_i + 1];

            PointMetadata metadata_a = in_stroke->metadata[point_i];
            PointMetadata metadata_b = in_stroke->metadata[point_i + 1];

            // Very conservative...
            b32 inside = !((a.x > canvas_rect.right     && b.x > canvas_rect.right) ||
                           (a.x < canvas_rect.left      && b.x < canvas_rect.left) ||
                           (a.y < canvas_rect.top       && b.y < canvas_rect.top) ||
                           (a.y > canvas_rect.bottom    && b.y > canvas_rect.bottom));

            // We can add the segment
            if (inside)
            {
                Stroke* stroke = &clipped_stroke->stroke;
                // XA AB<- CD
                // We need to add AB to out clipped stroke.
                // Based on the previous iteration:
                //  If we added, XA, then A is already inside; so for AB, we only need to add B.
                //  If XA was discarded, we haven't added A; both points need to be added.
                if (!added_previous_segment)
                {
                    stroke->points  [stroke->num_points] = a;
                    stroke->metadata[stroke->num_points] = metadata_a;
                    stroke->points  [stroke->num_points + 1] = b;
                    stroke->metadata[stroke->num_points + 1] = metadata_b;
                    stroke->num_points += 2;
                }
                else if (added_previous_segment)
                {
                    stroke->points  [stroke->num_points] = b;
                    stroke->metadata[stroke->num_points] = metadata_b;
                    stroke->num_points += 1;
                }
                added_previous_segment = true;
            }
            else
            {
                added_previous_segment = false;
            }
        }
    }
    return clipped_stroke;
}

func b32 is_rect_filled_by_stroke(Rect rect, v2i reference_point,
                                  v2i* points, i32 num_points,
                                  PointMetadata* metadata,
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
        for (i32 point_i = 0; point_i < num_points - 1; ++point_i)
        {
            v2i a = points[point_i];
            v2i b = points[point_i + 1];

            a = sub_v2i(points[point_i], reference_point);
            b = sub_v2i(points[point_i + 1], reference_point);

            // Get closest point
            // TODO: Refactor into something clearer after we're done.
            v2f ab = {(f32)(b.x - a.x), (f32)(b.y - a.y)};
            f32 mag_ab2 = ab.x * ab.x + ab.y * ab.y;
            v2i p  = closest_point_in_segment( a, b, ab, mag_ab2, rect_center, NULL);

            f32 p_a = metadata[point_i    ].pressure;
            f32 p_b = metadata[point_i + 1].pressure;

            f32 pressure = min(p_a, p_b);

            // Back to global coordinates
            p = add_v2i(p, reference_point);

            // Half width of a rectangle contained by brush at point p.
            i32 rad = (i32)(brush.radius  * 0.707106781f * pressure);  // cos(pi/4)
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

// Sample dx, dy, which are distances from the sampling point to the center of
// the circle, with 4x4 MSAA
// Returns the number of samples gathered.
func u32 sample_circle(CanvasView* view,
                       i32 downsample_factor, i32 local_scale,
                       f32 dx, f32 dy, u32 radius)
{
    u32 samples = 0;
    f32 f3 = (0.75f * view->scale) * downsample_factor * local_scale;
    f32 f1 = (0.25f * view->scale) * downsample_factor * local_scale;
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
        for (int i = 0; i < 16; ++i)
        {
            samples += (sqrtf(fdists[i]) < radius);
        }
    }
    else
    {
        u32 sq_radius = radius * radius;


        for (int i = 0; i < 16; ++i)
        {
            samples += (fdists[i] < sq_radius);
        }
    }
    return samples;
}

// returns false if allocation failed
func b32 rasterize_canvas_block(Arena* render_arena,
                                CanvasView* view,
                                Stroke* strokes,
                                b32* stroke_masks,
                                i32 num_strokes,
                                Stroke* working_stroke,
                                u32* pixels,
                                Rect raster_block)
{
    //u64 pre_ccount_begin = __rdtsc();
    Rect canvas_block;
    {
        canvas_block.top_left  = raster_to_canvas(view, raster_block.top_left);
        canvas_block.bot_right = raster_to_canvas(view, raster_block.bot_right);
    }

    // Not actually infinite! Draw programmer magenta when outside of bounds.
    if (canvas_block.left   < -view->canvas_radius_limit ||
        canvas_block.right  > view->canvas_radius_limit  ||
        canvas_block.top    < -view->canvas_radius_limit ||
        canvas_block.bottom > view->canvas_radius_limit
        )
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

    ClippedStroke* stroke_list = NULL;

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

    // Fill a linked list, filtering input strokes and clipping them to the current block.
    for (i32 stroke_i = 0; stroke_i <= num_strokes; ++stroke_i)
    {
        if (stroke_i < num_strokes && !stroke_masks[stroke_i])
        {
            continue;
        }
        Stroke* complete_stroke = NULL;
        if (stroke_i == num_strokes)
        {
            complete_stroke = working_stroke;
        }
        else
        {
            complete_stroke = &strokes[stroke_i];
        }
        assert(complete_stroke);
        Rect enlarged_block = rect_enlarge(canvas_block, complete_stroke->brush.radius);
        ClippedStroke* clipped_stroke = stroke_clip_to_rect(render_arena,
                                                            complete_stroke, enlarged_block);
        // ALlocation failed.
        // Handle this gracefully; this will cause more memory for render workers.
        if (!clipped_stroke)
        {
            return false;
        }

        Stroke* stroke = &clipped_stroke->stroke;

        if (stroke->num_points)
        {
            assert (stroke->brush.radius > 0);
            ClippedStroke* list_head = clipped_stroke;
            list_head->next = stroke_list;
        // TODO enable
#if 0
            if (is_rect_filled_by_stroke(
                        canvas_block, reference_point,
                        stroke->points, stroke->num_points,
                        stroke->metadata, stroke->brush,
                        view))
            {
                //list_head->fills_block = true;
            }
#endif
            stroke_list = list_head;
        }
    }

    // Avoid losing floating point precission at very close zoom levels.
    i32 local_scale = (view->scale <= 8) ? 4 : 1;

    {
        reference_point.x *= local_scale;
        reference_point.y *= local_scale;
    }

    // TODO enable
    // Set our `stroke_list` to end at the first opaque stroke that fills
    // this block.
#if 0
    ClippedStroke* list_iter = stroke_list;
    while (list_iter)
    {
        if (list_iter->fills_block && list_iter->stroke.brush.color.a == 1.0f)
        {
            list_iter->next = NULL;
            break;
        }
        list_iter = list_iter->next;
    }
#endif

    i32 downsample_factor = view->downsampling_factor;  // Different names for the same thing.

    // This is the distance between two adjacent canvas pixels. Derived to
    // avoid expensive raster_to_canvas computations in the inner loop
    i32 canvas_jump = downsample_factor * view->scale * local_scale;

    // i and j are the canvas point
    i32 j = (((raster_block.top - view->screen_center.y) *
              view->scale) - view->pan_vector.y) * local_scale - reference_point.y;

    for (i32 pixel_j = raster_block.top;
         pixel_j < raster_block.bottom;
         pixel_j += downsample_factor)
    {
        i32 i =  (((raster_block.left - view->screen_center.x) *
                    view->scale) - view->pan_vector.x) * local_scale - reference_point.x;

        for (i32 pixel_i = raster_block.left;
             pixel_i < raster_block.right;
             pixel_i += downsample_factor)
        {
            // Clear color
            v4f background_color = { 1, 1, 1, 1 };

            // Cumulative blending
            v4f acc_color = { 0 };

            ClippedStroke* list_iter = stroke_list;

            while(list_iter)
            {
                ClippedStroke* clipped_stroke = list_iter;
                list_iter = list_iter->next;
                Stroke* stroke = &clipped_stroke->stroke;

                assert (clipped_stroke);

                assert (stroke->num_points > 0);

                // Fast path.
                if (clipped_stroke->fills_block)
                {
#if 1 // Visualize it with black
                    v4f dst = {0,0,0,stroke->brush.color.a};
#else
                    v4f dst = stroke->brush.color;
#endif
                    acc_color = blend_v4f(dst, acc_color);
                }
                else
                // Slow path. There are pixels not inside.
                {
                    v2i* points = stroke->points;

                    i32 batch_size = 0;  // Up to 4. How many points could we load from the stroke.

                    //v2f min_points[4] = {0};
                    f32 min_dist = FLT_MAX;
                    f32 dx = 0;
                    f32 dy = 0;
                    f32 pressure = 0.0f;

                    if (stroke->num_points == 1)
                    {
                        v2i first_point = points[0];
                        {
                            first_point.x *= local_scale;
                            first_point.y *= local_scale;
                        }
                        //min_points[0] = v2i_to_v2f(sub_v2i(first_point, reference_point));
                        v2i min_point = sub_v2i(first_point, reference_point);
                        dx = (f32) (i - min_point.x);
                        dy = (f32) (j - min_point.y);
                        min_dist = dx * dx + dy * dy;
                    }
                    else
                    {
                        batch_size = 1;
                        for (int point_i = 0; point_i < stroke->num_points-1; ++point_i)
                        {
                            v2i point = points[point_i];
                            {
                                point.x *= local_scale;
                                point.y *= local_scale;
                            }
                            point = sub_v2i(point, reference_point);

                            f32 test_dx = (f32) (i - point.x);
                            f32 test_dy = (f32) (j - point.y);
                            f32 dist = test_dx * test_dx + test_dy * test_dy;
                            if (dist < min_dist)
                            {
                                min_point_i = point_i;
                                min_dist = dist;
                                dx = test_dx;
                                dy = test_dy;
                                pressure = stroke->metadata[point_i].pressure;
                            }
                        }
                    }

                    if (min_dist < FLT_MAX)
                    {
                        //u64 kk_ccount_begin = __rdtsc();
                        u32 radius = (u32)(stroke->brush.radius * local_scale * pressure);

                        // Sample the brush at the closest point.
                        u32 joint_samples = sample_circle(view, downsample_factor, local_scale,
                                                          dx, dy, radius);


                        // Now sample the frustums between points, interpolating.

                        // We have a left-segment
                        if (min_point_i > 0)
                        {

                        }
                        // We have a segment to the right
                        if (min_point_i < stroke->num_points)
                        {

                        }
                        u32 samples = joint_samples;

                        // If the stroke contributes to the pixel, do compositing.
                        if (samples > 0)
                        {
                            // Do blending
                            // ---------------

                            f32 coverage = (f32)joint_samples / 16.0f;

                            v4f dst = stroke->brush.color;
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

func void rasterize_ring(u32* pixels,
                         i32 width, i32 height,
                         i32 center_x, i32 center_y,
                         i32 ring_radius, i32 ring_girth,
                         v4f color)
{
#define compare(dist) \
    dist < square(ring_radius + ring_girth) && \
    dist > square(ring_radius - ring_girth)
#define distance(i, j) \
    ((i) - center_x) * ((i) - center_x) + ((j) - center_y) * ((j) - center_y)

    assert(ring_radius < (1 << 16));

    // TODO: Compute bounding box(es) for ring.

    for (i32 j = 0; j < height; ++j)
    {
        for (i32 i = 0; i < width; ++i)
        {
            // Rotated grid AA
            int samples = 0;
            {
                f32 u = 0.223607f;
                f32 v = 0.670820f;

                f32 fi = (f32)i;
                f32 fj = (f32)j;

                samples += compare(distance(fi - u, fj - v));
                samples += compare(distance(fi - v, fj + u));
                samples += compare(distance(fi + u, fj + v));
                samples += compare(distance(fi + v, fj + u));
            }

            if (samples > 0)
            {
                f32 contrib = (f32)samples / 4.0f;
                v4f aa_color = to_premultiplied(color.rgb, contrib);
                v4f dst = color_u32_to_v4f(pixels[j*width + i]);
                v4f blended = blend_v4f(dst, aa_color);
                pixels[j*width + i] = color_v4f_to_u32(blended);
            }
        }
    }
#undef compare
#undef distance
}

func void rasterize_color_picker(ColorPicker* picker,
                                 Rect draw_rect)
{
    // Wheel
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 picker_i =
                    (j - draw_rect.top) *( 2*picker->bounds_radius_px ) + (i - draw_rect.left);
            v2f point = {(f32)i, (f32)j};
            u32 dest_color = picker->pixels[picker_i];

            int samples = 0;
            {
                f32 u = 0.223607f;
                f32 v = 0.670820f;

                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){-u, -v}));
                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){-v, u}));
                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){u, v}));
                samples += (int)picker_hits_wheel(picker, add_v2f(point, (v2f){v, u}));
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
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            v2f point = { (f32)i, (f32)j };
            u32 picker_i =
                    (j - draw_rect.top) *( 2*picker->bounds_radius_px ) + (i - draw_rect.left);
            u32 dest_color = picker->pixels[picker_i];
            // MSAA!!
            int samples = 0;
            {
                f32 u = 0.223607f;
                f32 v = 0.670820f;

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

        v3f hsv = picker->hsv;

        v3f rgb = hsv_to_rgb(hsv);

        v4f color =
        {
            1 - rgb.r,
            1 - rgb.g,
            1 - rgb.b,
            1,
        };

        if (picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE)
        {
            ring_radius = 10;
            ring_girth  = 2;
            color       = (v4f){0};
        }

        // Barycentric to cartesian
        f32 a = hsv.s;
        f32 b = 1 - hsv.v;
        f32 c = 1 - a - b;

        v2f point = add_v2f(scale_v2f(picker->c, a),
                            add_v2f(scale_v2f(picker->b, b),
                                    scale_v2f(picker->a, c)));

        // De-center
        point.x -= picker->center.x - picker->bounds_radius_px;
        point.y -= picker->center.y - picker->bounds_radius_px;

        i32 width = picker->bounds_radius_px * 2;
        i32 height = picker->bounds_radius_px * 2;

        rasterize_ring(picker->pixels, width, height,
                       (i32)point.x, (i32)point.y,
                       ring_radius, ring_girth,
                       color);
    }
}

// Returns false if there were allocation errors
func b32 render_tile(MiltonState* milton_state,
                     Arena* tile_arena,
                     Rect* blocks,
                     i32 block_start, i32 num_blocks,
                     u32* raster_buffer)
{
    b32 allocation_ok = true;

    Rect raster_tile_rect = { 0 };
    Rect canvas_tile_rect = { 0 };

    const i32 blocks_per_tile = milton_state->blocks_per_tile;
    // Clip and move to canvas space.
    // Derive tile_rect
    for (i32 block_i = 0; block_i < blocks_per_tile; ++block_i)
    {
        if (block_start + block_i >= num_blocks)
        {
            break;
        }
        blocks[block_start + block_i] = rect_clip_to_screen(blocks[block_start + block_i],
                                                  milton_state->view->screen_size);
        raster_tile_rect = rect_union(raster_tile_rect, blocks[block_start + block_i]);

        canvas_tile_rect.top_left =
                raster_to_canvas(milton_state->view, raster_tile_rect.top_left);
        canvas_tile_rect.bot_right =
                raster_to_canvas(milton_state->view, raster_tile_rect.bot_right);
    }

    // Filter strokes to this tile.
    b32* stroke_masks = filter_strokes_to_rect(tile_arena,
                                               milton_state->strokes,
                                               milton_state->num_strokes,
                                               canvas_tile_rect);

    if (!stroke_masks)
    {
        allocation_ok = false;
    }

    for (int block_i = 0; block_i < blocks_per_tile && allocation_ok; ++block_i)
    {
        if (block_start + block_i >= num_blocks)
        {
            break;
        }

        allocation_ok = rasterize_canvas_block(tile_arena,
                                               milton_state->view,
                                               milton_state->strokes,
                                               stroke_masks,
                                               milton_state->num_strokes,
                                               &milton_state->working_stroke,
                                               raster_buffer,
                                               blocks[block_start + block_i]);
    }
    return allocation_ok;
}

typedef struct
{
    MiltonState* milton_state;
    i32 worker_id;
} WorkerParams;

func int render_worker(void* data)
{
    WorkerParams* params = (WorkerParams*) data;
    MiltonState* milton_state = params->milton_state;
    i32 id = params->worker_id;
    RenderQueue* render_queue = milton_state->render_queue;

    for (;;)
    {
        int err = SDL_SemWait(render_queue->work_available);
        if (err != 0)
        {
            milton_fatal("Failure obtaining semaphore inside render worker");
        }
        err = SDL_LockMutex(render_queue->mutex);
        if (err != 0)
        {
            milton_fatal("Failure locking render queue mutex");
        }
        i32 index = -1;
        TileRenderData tile_data = { 0 };
        {
            index = --render_queue->index;
            assert (index >= 0);
            assert (index <  RENDER_QUEUE_SIZE);
            tile_data = render_queue->tile_render_data[index];
            SDL_UnlockMutex(render_queue->mutex);
        }

        assert (index >= 0);

        b32 allocation_ok = render_tile(milton_state,
                                        &milton_state->render_worker_arenas[id],
                                        render_queue->blocks,
                                        tile_data.block_start, render_queue->num_blocks,
                                        render_queue->raster_buffer);
        if (!allocation_ok)
        {
            milton_state->worker_needs_memory = true;
        }

        arena_reset(&milton_state->render_worker_arenas[id]);

        SDL_SemPost(render_queue->completed_semaphore);
    }
}

func void produce_render_work(MiltonState* milton_state, TileRenderData tile_render_data)
{
    RenderQueue* render_queue = milton_state->render_queue;
    i32 lock_err = SDL_LockMutex(milton_state->render_queue->mutex);
    if (!lock_err)
    {
        render_queue->tile_render_data[render_queue->index++] = tile_render_data;
        SDL_UnlockMutex(render_queue->mutex);
    }
    else { assert (!"Lock failure not handled"); }

    SDL_SemPost(render_queue->work_available);
}


// Returns true if operation was completed.
func b32 render_canvas(MiltonState* milton_state, u32* raster_buffer, Rect raster_limits)
{
    Rect* blocks = NULL;

    i32 num_blocks = rect_split(milton_state->transient_arena,
                                raster_limits,
                                milton_state->block_width, milton_state->block_width, &blocks);

    if (num_blocks < 0)
    {
        milton_log ("[ERROR] Transient arena did not have enough memory for canvas block.\n");
        milton_fatal("Not handling this error.");
    }

    RenderQueue* render_queue = milton_state->render_queue;
    {
        render_queue->blocks = blocks;
        render_queue->num_blocks = num_blocks;
        render_queue->raster_buffer = raster_buffer;
    }

    const i32 blocks_per_tile = milton_state->blocks_per_tile;

    i32 tile_acc = 0;
    for (int block_i = 0; block_i < num_blocks; block_i += blocks_per_tile)
    {
        if (block_i >= num_blocks)
        {
            break;
        }

#define RENDER_MULTITHREADED 0
#if RENDER_MULTITHREADED
        TileRenderData data =
        {
            .block_start = block_i,
        };

        produce_render_work(milton_state, data);
        tile_acc += 1;
#else
        Arena tile_arena = arena_push(milton_state->transient_arena,
                                      arena_available_space(milton_state->transient_arena));
        render_tile(milton_state,
                    &tile_arena,
                    blocks,
                    block_i, num_blocks,
                    raster_buffer);

        arena_pop(&tile_arena);
#endif
        ARENA_VALIDATE(milton_state->transient_arena);
    }

#if RENDER_MULTITHREADED
    // Wait for workers to finish.

    while(tile_acc)
    {
        i32 waited_err = SDL_SemWait(milton_state->render_queue->completed_semaphore);
        if (!waited_err)
        {
            --tile_acc;
        }
        else { assert ( !"Not handling completion semaphore wait error" ); }
    }
#endif

    ARENA_VALIDATE(milton_state->transient_arena);

    return true;
}

func void render_picker(ColorPicker* picker,
                        u32* buffer_pixels, CanvasView* view)
{
    v4f background_color =
    {
        0.5f,
        0.5f,
        0.55f,
        0.4f,
    };
    background_color = to_premultiplied(background_color.rgb, background_color.a);

    Rect draw_rect = picker_get_bounds(picker);

    // Copy canvas buffer into picker buffer
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 picker_i =
                    (j - draw_rect.top) * (2*picker->bounds_radius_px ) + (i - draw_rect.left);
            u32 src = buffer_pixels[j * view->screen_size.w + i];
            picker->pixels[picker_i] = src;
        }
    }

    // Render background color.
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 picker_i =
                    (j - draw_rect.top) *( 2*picker->bounds_radius_px ) + (i - draw_rect.left);


            // TODO: Premultiplied?
            v4f dest = color_u32_to_v4f(picker->pixels[picker_i]);

            v4f result = blend_v4f(dest, background_color);

            picker->pixels[picker_i] = color_v4f_to_u32(result);
        }
    }

    rasterize_color_picker(picker, draw_rect);

    // Blit picker pixels
    u32* to_blit = picker->pixels;
    for (int j = draw_rect.top; j < draw_rect.bottom; ++j)
    {
        for (int i = draw_rect.left; i < draw_rect.right; ++i)
        {
            u32 linear_color = *to_blit++;
            v4f rgba = color_u32_to_v4f(linear_color);
            u32 color = color_v4f_to_u32(rgba);

            buffer_pixels[j * view->screen_size.w + i] = color;
        }
    }
}

func void render_brush_overlay(MiltonState* milton_state, v2i hover_point)
{
    // TODO: Remove the two buffers. Async will not need it.
    u32* buffer = (u32*)milton_state->raster_buffers[milton_state->raster_buffer_index];

    i32 girth = 2;

    if (milton_get_brush_size(milton_state) <= girth / 2)
    {
        return;
    }

    i32 radius = milton_get_brush_size(milton_state);
    assert(radius == milton_state->working_stroke.brush.radius / milton_state->view->scale);

    rasterize_ring(buffer,
                   milton_state->view->screen_size.w,
                   milton_state->view->screen_size.h,
                   hover_point.x, hover_point.y,
                   radius, girth,
                   (v4f) { 0 });
}

typedef enum
{
    MiltonRenderFlags_NONE              = 0,
    MiltonRenderFlags_PICKER_UPDATED    = (1 << 0),
    MiltonRenderFlags_FULL_REDRAW       = (1 << 1),
    MiltonRenderFlags_BRUSH_OVERLAY     = (1 << 2),
} MiltonRenderFlags;

func void milton_render(MiltonState* milton_state, MiltonRenderFlags render_flags)
{
    // `raster_limits` is the part of the screen (in pixels) that should be updated
    // with what's on the canvas.
    Rect raster_limits = { 0 };

    // Figure out what `raster_limits` should be.
    {
        if (render_flags & MiltonRenderFlags_FULL_REDRAW)
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
                    stroke->points[stroke->num_points - 2]);

            raster_limits.left   = min (milton_state->last_raster_input.x, new_point.x);
            raster_limits.right  = max (milton_state->last_raster_input.x, new_point.x);
            raster_limits.top    = min (milton_state->last_raster_input.y, new_point.y);
            raster_limits.bottom = max (milton_state->last_raster_input.y, new_point.y);
            i32 block_offset = 0;
            i32 w = raster_limits.right - raster_limits.left;
            i32 h = raster_limits.bottom - raster_limits.top;
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
            v2i point = canvas_to_raster(milton_state->view, stroke->points[0]);
            i32 raster_radius = stroke->brush.radius / milton_state->view->scale;
            raster_radius = max(raster_radius, milton_state->block_width);
            raster_limits.left   = -raster_radius + point.x;
            raster_limits.right  = raster_radius  + point.x;
            raster_limits.top    = -raster_radius + point.y;
            raster_limits.bottom = raster_radius  + point.y;
            raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);
            assert (raster_limits.right >= raster_limits.left);
            assert (raster_limits.bottom >= raster_limits.top);
        }
    }

    //i32 index = (milton_state->raster_buffer_index + 1) % 2;
    i32 index = (milton_state->raster_buffer_index + 0);
    u32* raster_buffer = (u32*)milton_state->raster_buffers[index];

#if 0
    // DEBUG always render the whole thing.
    {
        raster_limits.left = 0;
        raster_limits.right = milton_state->view->screen_size.w;
        raster_limits.top = 0;
        raster_limits.bottom = milton_state->view->screen_size.h;
    }
#endif

    b32 completed = render_canvas(milton_state, raster_buffer, raster_limits);

    // Render UI
    if (completed)
    {
        b32 redraw = false;
        Rect picker_rect = picker_get_bounds(&milton_state->picker);
        Rect clipped = rect_intersect(picker_rect, raster_limits);
        if ((clipped.left != clipped.right) && clipped.top != clipped.bottom)
        {
            redraw = true;
        }

        if (redraw || (render_flags & MiltonRenderFlags_PICKER_UPDATED))
        {
            render_canvas(milton_state, raster_buffer, picker_rect);

            render_picker(&milton_state->picker,
                          raster_buffer,
                          milton_state->view);
        }
    }

    /* if (render_flags & MiltonRenderFlags_BRUSH_OVERLAY) */
    /* { */
    /*     render_brush_overlay(milton_state, milton_state->hover_point); */
    /* } */

    // If not preempted, do a buffer swap.
    if (completed)
    {
        i32 prev_index = milton_state->raster_buffer_index;
        milton_state->raster_buffer_index = index;

        memcpy(milton_state->raster_buffers[prev_index],
               milton_state->raster_buffers[index],
               milton_state->max_width * milton_state->max_height * milton_state->bytes_per_pixel);
    }
}
