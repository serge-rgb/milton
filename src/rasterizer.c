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

// TODO: There should be a way to deal with high density displays.

// We might want to split this file into
//  - Actual rasterization of implicitly defined geometry (including strokes.)
//  - Render jobs and spacial division
//  - Bitmap loading and resource management.


#include "gui.h"

typedef struct ClippedStroke_s ClippedStroke;

struct ClippedStroke_s {
    // A bool inside a struct whose only purpose is to make things faster. This
    // would probably make Mike Acton angry, but it works really well.
    // What it does: It is set to true when this clipped stroke completely
    // fills the block to which it's clipped. This allows us to avoid a *lot*
    // of work by just flood-filling the block.
    b32             fills_block;

    // This stroke is clipped to a particular block and its pointers only live
    // as long as the render worker lasts. It's a direct memcpy from whatever
    // stroke we are clipping.  I tried using a smarter structure that did not
    // copy too much memory but it ended up being significantly slower
    Stroke          stroke;

    // We store indices into clipped points to delineate segments.  i.e. We
    // know that ab is a segment if index(a) + 1 == index(b)
    //
    // The alternative is to store points as AB BC CD DE, basically doubling
    // the memory requirement.  This solution turned out to not have a
    // significant perf impact and reduce dramatically the memory needed by
    // render workers.
    i32*            indices;

    // Yes, it is a linked list. Shoot me.
    //
    // In my defense, the elements of this list are guaranteed to be allocated
    // sequentially.
    ClippedStroke*  next;
};

static ClippedStroke* stroke_clip_to_rect(Arena* render_arena, Stroke* in_stroke, Rect canvas_rect)
{
    ClippedStroke* clipped_stroke = arena_alloc_elem(render_arena, ClippedStroke);
    if (!clipped_stroke) {
        return NULL;
    }

    clipped_stroke->fills_block = false;
    clipped_stroke->next = NULL;
    clipped_stroke->stroke = *in_stroke;  // Copy everything except:

    // ... now substitute the point data with an array of our own.
    if (in_stroke->num_points > 0) {
        clipped_stroke->stroke.num_points = 0;
        clipped_stroke->stroke.points = arena_alloc_array(render_arena,
                                                          in_stroke->num_points, v2i);
        clipped_stroke->stroke.metadata = arena_alloc_array(render_arena,
                                                            in_stroke->num_points, PointMetadata);
        clipped_stroke->indices = arena_alloc_array(render_arena,
                                                    in_stroke->num_points, i32);
    } else {
        return clipped_stroke;
    }


    if ( !clipped_stroke->stroke.points ||
         !clipped_stroke->stroke.metadata ||
         !clipped_stroke->indices ) {
        // We need more memory. Return gracefully
        return NULL;
    }
    if ( in_stroke->num_points == 1 ) {
        if (is_inside_rect(canvas_rect, in_stroke->points[0])) {
            Stroke* stroke = &clipped_stroke->stroke;
            i32 index = stroke->num_points++;
            stroke->points[index] = in_stroke->points[0];
            stroke->metadata[index] = in_stroke->metadata[0];
        }
    } else {
        i32 num_points = in_stroke->num_points;
        b32 added_previous_segment = false;
        for (i32 point_i = 0; point_i < num_points - 1; ++point_i) {
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
            if ( inside ) {
                Stroke* stroke = &clipped_stroke->stroke;
                // XA AB<- CD
                // We need to add AB to out clipped stroke.
                // Based on the previous iteration:
                //  If we added, XA, then A is already inside; so for AB, we only need to add B.
                //  If XA was discarded, we haven't added A; both points need to be added.
                if ( !added_previous_segment ) {
                    stroke->points[stroke->num_points]     = a;
                    stroke->points[stroke->num_points + 1] = b;

                    stroke->metadata[stroke->num_points]     = metadata_a;
                    stroke->metadata[stroke->num_points + 1] = metadata_b;

                    clipped_stroke->indices[stroke->num_points]     = point_i;
                    clipped_stroke->indices[stroke->num_points + 1] = point_i + 1;

                    stroke->num_points += 2;
                } else if ( added_previous_segment ) {
                    stroke->points  [stroke->num_points]        = b;
                    stroke->metadata[stroke->num_points]        = metadata_b;
                    clipped_stroke->indices[stroke->num_points] = point_i + 1;
                    stroke->num_points += 1;
                }
                added_previous_segment = true;
            } else {
                added_previous_segment = false;
            }
        }
    }
    return clipped_stroke;
}

static b32 is_rect_filled_by_stroke(Rect rect, v2i reference_point,
                                    v2i* points, i32* indices,
                                    i32 num_points,
                                    PointMetadata* metadata,
                                    Brush brush, CanvasView* view)
{
    assert ((((rect.left + rect.right) / 2) - reference_point.x) == 0);
    assert ((((rect.top + rect.bottom) / 2) - reference_point.y) == 0);
#if 0
    // Perf note: With the current use, this is actually going to be zero.
    v2i rect_center =
    {
        ((rect.left + rect.right) / 2) - reference_point.x,
        ((rect.top + rect.bottom) / 2) - reference_point.y,
    };
#endif

    if (num_points >= 2) {
        for ( i32 point_i = 0; point_i < num_points - 1; ++point_i ) {
            if ( indices[point_i] + 1 != indices[point_i + 1] ) {
                continue;
            }
            v2i a = points[point_i];
            v2i b = points[point_i + 1];

            a = sub_v2i(points[point_i], reference_point);
            b = sub_v2i(points[point_i + 1], reference_point);

            // Get closest point
            // TODO: Refactor into something clearer after we're done.
            v2f ab = {(f32)(b.x - a.x), (f32)(b.y - a.y)};
            f32 mag_ab2 = ab.x * ab.x + ab.y * ab.y;
            v2i p  = closest_point_in_segment(a, b, ab, mag_ab2,(v2i){0}, NULL);

            f32 p_a = metadata[point_i    ].pressure;
            f32 p_b = metadata[point_i + 1].pressure;

            f32 pressure = min(p_a, p_b);

            // Back to global coordinates
            p = add_v2i(p, reference_point);

            // Half width of a rectangle contained by brush at point p.
            i32 rad = (i32)(brush.radius  * 0.707106781f * pressure);  // cos(pi/4)
            Rect bounded_rect; {
                bounded_rect.left   = p.x - rad;
                bounded_rect.right  = p.x + rad;
                bounded_rect.bottom = p.y + rad;
                bounded_rect.top    = p.y - rad;
            }

            if ( is_rect_within_rect(rect, bounded_rect) ) {
                return true;
            }
        }
    } else if ( num_points == 1 ) {
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

        if ( is_rect_within_rect(rect, bounded_rect) ) {
            return true;
        }
    }
    return false;
}

// Fills a linked list of strokes that this block needs to render.
// Returns true if allocation succeeded, false if not.
static ClippedStroke* clip_strokes_to_block(Arena* render_arena,
                                          CanvasView* view,
                                          StrokeCord* strokes,
                                          b32* stroke_masks,
                                          Stroke* working_stroke,
                                          Rect canvas_block,
                                          v2i reference_point,
                                          b32* allocation_ok)
{
    ClippedStroke* stroke_list = NULL;
    *allocation_ok = true;
    i32 num_strokes = strokes->count;

    // Fill linked list with strokes clipped to this block
    for ( i32 stroke_i = 0; stroke_i <= num_strokes; ++stroke_i ) {
        if ( stroke_i < num_strokes && !stroke_masks[stroke_i] ) {
            // Stroke masks is of size num_strokes, but we use stroke_i ==
            // num_strokes to indicate the current "working stroke"
            continue;
        }
        Stroke* unclipped_stroke = NULL;
        if ( stroke_i == num_strokes ) {
            unclipped_stroke = working_stroke;
        } else {
            unclipped_stroke = StrokeCord_get(strokes, stroke_i);
        }
        assert(unclipped_stroke);
        Rect enlarged_block = rect_enlarge(canvas_block, unclipped_stroke->brush.radius);
        ClippedStroke* clipped_stroke = stroke_clip_to_rect(render_arena, unclipped_stroke, enlarged_block);
        // ALlocation failed.
        // Handle this gracefully; this will cause more memory for render workers.
        if ( !clipped_stroke ) {
            *allocation_ok = false;
            return NULL;
        }
        Stroke* stroke = &clipped_stroke->stroke;

        if ( stroke->num_points ) {
            ClippedStroke* list_head = clipped_stroke;
            list_head->next = stroke_list;
            if ( is_rect_filled_by_stroke(canvas_block, reference_point,
                                          stroke->points, clipped_stroke->indices,
                                          stroke->num_points,
                                          stroke->metadata, stroke->brush,
                                          view) ) {
                list_head->fills_block = true;
            }
            stroke_list = list_head;
        }
    }

    // Set our `stroke_list` to end at the first opaque stroke that fills
    // this block.
    ClippedStroke* list_iter = stroke_list;

    while (list_iter) {
        if ( list_iter->fills_block && list_iter->stroke.brush.color.a == 1.0f ) {
            list_iter->next = NULL;
            break;
        }
        list_iter = list_iter->next;
    }

    return stroke_list;
}

// returns false if allocation failed
static b32 rasterize_canvas_block_slow(Arena* render_arena,
                                       CanvasView* view,
                                       StrokeCord* strokes,
                                       b32* stroke_masks,
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

    if ( canvas_block.left   < -view->canvas_radius_limit ||
         canvas_block.right  > view->canvas_radius_limit  ||
         canvas_block.top    < -view->canvas_radius_limit ||
         canvas_block.bottom > view->canvas_radius_limit ) {
        for ( int j = raster_block.top; j < raster_block.bottom; j++ ) {
            for ( int i = raster_block.left; i < raster_block.right; i++ ) {
                pixels[j * view->screen_size.w + i] = 0xffff00ff;
            }
        }
        return true;
    }

// Leaving this toggle-able for a quick way to show the cool precision error.
    v2i reference_point =
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
    ClippedStroke* stroke_list = clip_strokes_to_block(render_arena, view,
                                                       strokes, stroke_masks, working_stroke,
                                                       canvas_block, reference_point,
                                                       &allocation_ok);
    if (!allocation_ok) {
        // Request more memory
        return false;
    }

    i32 local_scale =  (view->scale <= 8) ?  4 : 1;
    {
        reference_point.x *= local_scale;
        reference_point.y *= local_scale;
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
          pixel_j += downsample_factor ) {
        i32 i =  (((raster_block.left - view->screen_center.x) *
                    view->scale) - view->pan_vector.x) * local_scale - reference_point.x;

        for ( i32 pixel_i = raster_block.left;
              pixel_i < raster_block.right;
              pixel_i += downsample_factor ) {
            // Clear color
            v4f background_color = { 1, 1, 1, 1 };

            // Cumulative blending
            v4f acc_color = { 0 };

            ClippedStroke* list_iter = stroke_list;

            while(list_iter) {
                ClippedStroke* clipped_stroke = list_iter;
                Stroke* stroke = &clipped_stroke->stroke;
                list_iter = list_iter->next;
                assert (stroke->num_points > 0);

                // Fast path.
                if (clipped_stroke->fills_block) {
#if 0 // Visualize it with black
                    v4f dst = {0, 0, 0, stroke->brush.color.a};
#else
                    v4f dst = stroke->brush.color;
#endif
                    acc_color = blend_v4f(dst, acc_color);
                } else {
                // Slow path. There are pixels not inside.
                    v2i* points = stroke->points;

                    //v2f min_points[4] = {0};
                    f32 min_dist = FLT_MAX;
                    f32 dx = 0;
                    f32 dy = 0;
                    f32 pressure = 0.0f;

                    if ( stroke->num_points == 1 ) {
                        v2i first_point = points[0];
                        {
                            first_point.x *= local_scale;
                            first_point.y *= local_scale;
                        }
                        //min_points[0] = v2i_to_v2f(sub_v2i(first_point, reference_point));
                        v2i min_point = sub_v2i(first_point, reference_point);
                        dx = (f32)(i - min_point.x);
                        dy = (f32)(j - min_point.y);
                        min_dist = dx * dx + dy * dy;
                        pressure = stroke->metadata[0].pressure;
                    } else {
                        // Find closest point.
                        for ( int point_i = 0; point_i < stroke->num_points - 1; point_i++ ) {
                            i32 index_a = clipped_stroke->indices[point_i];
                            i32 index_b = clipped_stroke->indices[point_i + 1];
                            // Skip this iteration. This is not a segment.
                            if ( index_a + 1 != index_b ) {
                                continue;
                            }

                            v2i a = points[point_i];
                            v2i b = points[point_i + 1];
                            {
                                a.x *= local_scale;
                                a.y *= local_scale;
                                b.x *= local_scale;
                                b.y *= local_scale;
                            }
                            a = sub_v2i(a, reference_point);
                            b = sub_v2i(b, reference_point);

                            v2f ab = {(f32)(b.x - a.x), (f32)(b.y - a.y)};
                            f32 mag_ab2 = ab.x * ab.x + ab.y * ab.y;
                            if ( mag_ab2 > 0 ) {
                                f32 t;
                                v2f point = v2i_to_v2f(closest_point_in_segment(a, b,
                                                                                ab, mag_ab2,
                                                                                (v2i){i,j}, &t));
                                f32 test_dx = (f32) (i - point.x);
                                f32 test_dy = (f32) (j - point.y);
                                f32 dist = sqrtf(test_dx * test_dx + test_dy * test_dy);
                                f32 p_a = stroke->metadata[point_i    ].pressure;
                                f32 p_b = stroke->metadata[point_i + 1].pressure;
                                f32 test_pressure = (1 - t) * p_a + t * p_b;
                                dist = dist - test_pressure * stroke->brush.radius;
                                if ( dist < min_dist ) {
                                    min_dist = dist;
                                    dx = test_dx;
                                    dy = test_dy;
                                    pressure = test_pressure;
                                }
                            }
                        }
                    }

                    if ( min_dist < FLT_MAX ) {
                        // TODO: For implicit brush:
                        //  This sampling is for a circular brush.
                        //  Should dispatch on brush type. And do it for SSE impl too.
                        int samples = 0;
                        {
                            f32 f3 = (0.75f * view->scale) * downsample_factor * local_scale;
                            f32 f1 = (0.25f * view->scale) * downsample_factor * local_scale;
                            u32 radius = (u32)(stroke->brush.radius * pressure * local_scale);
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
                            if (radius >= ( 1 << 16 )) {
                                for ( int sample_i = 0; sample_i < 16; ++sample_i ) {
                                    samples += (sqrtf(fdists[sample_i]) < radius);
                                }
                            } else {
                                u32 sq_radius = radius * radius;

                                for ( int sample_i = 0; sample_i < 16; ++sample_i ) {
                                    samples += (fdists[sample_i] < sq_radius);
                                }
                            }
                        }

                        // If the stroke contributes to the pixel, do compositing.
                        if ( samples > 0 ) {
                            // Do blending
                            // ---------------

                            f32 coverage = (f32)samples / 16.0f;

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
                if ( acc_color.a > 0.999999 ) {
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

            for (i32 jj = pixel_j; jj < pixel_j + downsample_factor; ++jj) {
                for (i32 ii = pixel_i; ii < pixel_i + downsample_factor; ++ii) {
                    pixels[jj * view->screen_size.w + ii] = pixel;
                }
            }
            i += canvas_jump;
        }
        j += canvas_jump;
    }
    return true;
}

static b32 rasterize_canvas_block_sse2(Arena* render_arena,
                                       CanvasView* view,
                                       StrokeCord* strokes,
                                       b32* stroke_masks,
                                       Stroke* working_stroke,
                                       u32* pixels,
                                       Rect raster_block)
{

    __m128 one4 = _mm_set_ps1(1);
    __m128 zero4 = _mm_set_ps1(0);
    //u64 pre_ccount_begin = __rdtsc();
    Rect canvas_block;
    {
        canvas_block.top_left  = raster_to_canvas(view, raster_block.top_left);
        canvas_block.bot_right = raster_to_canvas(view, raster_block.bot_right);
    }

    if ( canvas_block.left   < -view->canvas_radius_limit ||
         canvas_block.right  > view->canvas_radius_limit  ||
         canvas_block.top    < -view->canvas_radius_limit ||
         canvas_block.bottom > view->canvas_radius_limit
        ) {
        for ( int j = raster_block.top; j < raster_block.bottom; j++ ) {
            for ( int i = raster_block.left; i < raster_block.right; i++ ) {
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
    ClippedStroke* stroke_list = clip_strokes_to_block(render_arena, view,
                                                       strokes, stroke_masks,
                                                       working_stroke,
                                                       canvas_block, reference_point,
                                                       &allocation_ok);
    if ( !allocation_ok ) {
        // Request more memory
        return false;
    }

    i32 local_scale =  (view->scale <= 8) ?  4 : 1;
    {
        reference_point.x *= local_scale;
        reference_point.y *= local_scale;
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
          pixel_j += downsample_factor ) {
        i32 i =  (((raster_block.left - view->screen_center.x) *
                    view->scale) - view->pan_vector.x) * local_scale - reference_point.x;

        for ( i32 pixel_i = raster_block.left;
              pixel_i < raster_block.right;
              pixel_i += downsample_factor ) {
            // Clear color
            v4f background_color = { 1, 1, 1, 1 };

            // Cumulative blending
            v4f acc_color = { 0 };

            ClippedStroke* list_iter = stroke_list;

            while(list_iter) {
                ClippedStroke* clipped_stroke = list_iter;
                Stroke* stroke = &clipped_stroke->stroke;
                list_iter = list_iter->next;
                assert (stroke->num_points > 0);

                // Fast path.
                if ( clipped_stroke->fills_block ) {
#if 0 // Visualize it with black
                    v4f dst = {0, 0, 0, stroke->brush.color.a};
#else
                    v4f dst = stroke->brush.color;
#endif
                    acc_color = blend_v4f(dst, acc_color);
                } else {
                    // Slow path. There are pixels not inside.
                    v2i* points = stroke->points;

                    f32 min_dist = FLT_MAX;
                    f32 dx = 0;
                    f32 dy = 0;
                    f32 pressure = 0.0f;

                    if ( stroke->num_points == 1 ) {
                        v2i first_point = points[0];
                        {
                            first_point.x *= local_scale;
                            first_point.y *= local_scale;
                        }
                        v2i min_point = sub_v2i(first_point, reference_point);
                        dx = (f32)(i - min_point.x);
                        dy = (f32)(j - min_point.y);
                        min_dist = dx * dx + dy * dy;
                        pressure = stroke->metadata[0].pressure;
                    } else {
//#define SSE_M(wide, i) ((f32 *)&(wide) + i)
                        for ( int point_i = 0; point_i < stroke->num_points - 1; point_i += 4 ) {
                            f32 axs[4] = { 0 };
                            f32 ays[4] = { 0 };
                            f32 bxs[4] = { 0 };
                            f32 bys[4] = { 0 };
                            f32 aps[4];
                            f32 bps[4];

                            // This is stupid.
                            //
                            // If we stored the strokes as in Struct of Arrays (SOA) form, we could avoid this loop.
                            //
                            // It would turn into the 6 _mm_set_ps calls
                            // followed by 4 wide muls and 4 wide adds.  One
                            // caveat: points would have to be a multiple of 4.
                            // Not so bad. We would also save the set-to-zero
                            // that we do above, which is expensive...

                            for ( i32 batch_i = 0; batch_i < 4; batch_i++ ) {
                                register i32 index = point_i + batch_i;

                                if (index + 1 >= stroke->num_points)
                                {
                                    break;
                                }

                                i32 index_a = clipped_stroke->indices[index];
                                i32 index_b = clipped_stroke->indices[index + 1];

                                // Not a segment
                                if ( index_a + 1 != index_b ) {
                                    continue;
                                }
                                // The point of reference point is to do the subtraction with
                                // integer arithmetic
                                axs[batch_i] = (f32)(points[index  ].x * local_scale - reference_point.x);
                                ays[batch_i] = (f32)(points[index  ].y * local_scale - reference_point.y);
                                bxs[batch_i] = (f32)(points[index+1].x * local_scale - reference_point.x);
                                bys[batch_i] = (f32)(points[index+1].y * local_scale - reference_point.y);
                                aps[batch_i] = stroke->metadata[index  ].pressure;
                                bps[batch_i] = stroke->metadata[index+1].pressure;
                            }

                            __m128 a_x = _mm_set_ps((axs[3]), (axs[2]), (axs[1]), (axs[0]));
                            __m128 b_x = _mm_set_ps((bxs[3]), (bxs[2]), (bxs[1]), (bxs[0]));
                            __m128 a_y = _mm_set_ps((ays[3]), (ays[2]), (ays[1]), (ays[0]));
                            __m128 b_y = _mm_set_ps((bys[3]), (bys[2]), (bys[1]), (bys[0]));
                            __m128 pressure_a = _mm_set_ps(aps[3], aps[2], aps[1], aps[0]);
                            __m128 pressure_b = _mm_set_ps(bps[3], bps[2], bps[1], bps[0]);

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

                            __m128 radius4 = _mm_set_ps1((f32)stroke->brush.radius);
                            dist4 = _mm_sub_ps(dist4, _mm_mul_ps(pressure4, radius4));

                            f32 dists[4];
                            f32 tests_dx[4];
                            f32 tests_dy[4];
                            f32 pressures[4];
                            f32 masks[4];

                            _mm_store_ps(dists, dist4);
                            _mm_store_ps(tests_dx, test_dx);
                            _mm_store_ps(tests_dy, test_dy);
                            _mm_store_ps(pressures, pressure4);
                            _mm_store_ps(masks, mask);

                            // Note: We can use 4 instead of batch_size.
                            // Because `mask` will be 0 for the invalid elements.
                            // But what we really want is to do this loop with
                            // SSE magic. .
                            for ( i32 batch_i = 0; batch_i < 4; ++batch_i ) {
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
                        }
                    }

                    if ( min_dist < FLT_MAX ) {
                        //u64 kk_ccount_begin = __rdtsc();
                        int samples = 0;
                        {
                            f32 f3 = (0.75f * view->scale) * downsample_factor * local_scale;
                            f32 f1 = (0.25f * view->scale) * downsample_factor * local_scale;
                            u32 radius = (u32)(stroke->brush.radius * pressure * local_scale);
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
                            //u32 radius = stroke->brush.radius;
                            //assert (radius > 0);
                            assert (radius < sqrtf((FLT_MAX)));

                            __m128 radius4 = _mm_set_ps1((f32)radius);

                            // Perf note: We remove the sqrtf call when it's
                            // safe to square the radius
                            __m128 comparisons[4];
                            __m128 ones = _mm_set_ps1(1.0f);
                            if ( radius >= (1 << 16) ) {
                                // sqrt slow. rsqrt fast
                                dists[0] = _mm_mul_ps(dists[0], _mm_rsqrt_ps(dists[0]));
                                dists[1] = _mm_mul_ps(dists[1], _mm_rsqrt_ps(dists[1]));
                                dists[2] = _mm_mul_ps(dists[2], _mm_rsqrt_ps(dists[2]));
                                dists[3] = _mm_mul_ps(dists[3], _mm_rsqrt_ps(dists[3]));
                                comparisons[0] = _mm_cmplt_ps(dists[0], radius4);
                                comparisons[1] = _mm_cmplt_ps(dists[1], radius4);
                                comparisons[2] = _mm_cmplt_ps(dists[2], radius4);
                                comparisons[3] = _mm_cmplt_ps(dists[3], radius4);
                            } else {
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
                        if ( samples > 0 ) {
                            // Do blending
                            // ---------------

                            f32 coverage = (f32)samples / 16.0f;

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
                if ( acc_color.a > 0.999999 ) {
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

            for ( i32 jj = pixel_j; jj < pixel_j + downsample_factor; ++jj ) {
                for ( i32 ii = pixel_i; ii < pixel_i + downsample_factor; ++ii ) {
                    pixels[jj * view->screen_size.w + ii] = pixel;
                }
            }
            i += canvas_jump;
        }
        j += canvas_jump;
    }
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

    for ( i32 j = top; j < bottom; ++j ) {
        for ( i32 i = left; i < right; ++i ) {
            // Rotated grid AA
            int samples = 0;
            {
                f32 u = 0.223607f;
                f32 v = 0.670820f;

                f32 fi = (f32)i;
                f32 fj = (f32)j;

                samples += COMPARE(DISTANCE(fi - u, fj - v));
                samples += COMPARE(DISTANCE(fi - v, fj + u));
                samples += COMPARE(DISTANCE(fi + u, fj + v));
                samples += COMPARE(DISTANCE(fi + v, fj + u));
            }

            if ( samples > 0 ) {
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

static void draw_circle(u32* raster_buffer,
                        i32 raster_buffer_width, i32 raster_buffer_height,
                        i32 center_x, i32 center_y,
                        i32 radius,
                        v4f src_color)
{
    i32 left = max(center_x - radius, 0);
    i32 right = min(center_x + radius, raster_buffer_width);
    i32 top = max(center_y - radius, 0);
    i32 bottom = min(center_y + radius, raster_buffer_height);

    assert (right >= left);
    assert (bottom >= top);

    for ( i32 j = top; j < bottom; ++j ) {
        for ( i32 i = left; i < right; ++i ) {
            i32 index = j * raster_buffer_width + i;


            //TODO: AA
            f32 dist = distance(((v2f){ (f32)i, (f32)j }),
                                (v2f){ (f32)center_x, (f32)center_y });

            if (dist < radius) {
                u32 dst_color = raster_buffer[index];
                v4f blended = blend_v4f(color_u32_to_v4f(dst_color), src_color);
                u32 color = color_v4f_to_u32(blended);
                raster_buffer[index] = color;
            }
        }
    }
}
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

    for ( i32 j = top; j < bottom; ++j ) {
        for ( i32 i = left; i < right; ++i ) {
            i32 index = j * raster_buffer_width + i;

            u32 dst_color = raster_buffer[index];
            v4f blended = blend_v4f(color_u32_to_v4f(dst_color), src_color);
            u32 color = color_v4f_to_u32(blended);
            raster_buffer[index] = color;
        }
    }
}

// 1-pixel margin
static void draw_rectangle_with_margin(u32* raster_buffer,
                                       i32 raster_buffer_width, i32 raster_buffer_height,
                                       i32 center_x, i32 center_y,
                                       i32 rect_w, i32 rect_h,
                                       v4f rect_color, v4f margin_color)
{
    i32 left = max(center_x - rect_w, 0);
    i32 right = min(center_x + rect_w, raster_buffer_width);
    i32 top = max(center_y - rect_h, 0);
    i32 bottom = min(center_y + rect_h, raster_buffer_height);

    assert (right >= left);
    assert (bottom >= top);

    for ( i32 j = top; j < bottom; ++j ) {
        for ( i32 i = left; i < right; ++i ) {
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


static void rasterize_color_picker(ColorPicker* picker,
                                   Rect draw_rect)
{
    // Wheel
    for ( int j = draw_rect.top; j < draw_rect.bottom; ++j ) {
        for ( int i = draw_rect.left; i < draw_rect.right; ++i ) {
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

            if (samples > 0) {
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
    for ( int j = draw_rect.top; j < draw_rect.bottom; ++j ) {
        for ( int i = draw_rect.left; i < draw_rect.right; ++i ) {
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
                                                   picker->info.a, picker->info.b, picker->info.c);
                samples += (int)is_inside_triangle(add_v2f(point, (v2f){-v, u}),
                                                   picker->info.a, picker->info.b, picker->info.c);
                samples += (int)is_inside_triangle(add_v2f(point, (v2f){u, v}),
                                                   picker->info.a, picker->info.b, picker->info.c);
                samples += (int)is_inside_triangle(add_v2f(point, (v2f){v, u}),
                                                   picker->info.a, picker->info.b, picker->info.c);
            }

            if (samples > 0) {
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

        v3f hsv = picker->info.hsv;

        v3f rgb = hsv_to_rgb(hsv);

        v4f color = {
            1 - rgb.r,
            1 - rgb.g,
            1 - rgb.b,
            1,
        };

        if (picker->flags & ColorPickerFlags_TRIANGLE_ACTIVE) {
            ring_radius = 10;
            ring_girth  = 2;
            color       = (v4f){0};
        }

        // Barycentric to cartesian
        f32 a = hsv.s;
        f32 b = 1 - hsv.v;
        f32 c = 1 - a - b;

        v2f point = add_v2f(scale_v2f(picker->info.c, a),
                            add_v2f(scale_v2f(picker->info.b, b),
                                    scale_v2f(picker->info.a, c)));

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

// Returns false if there were allocation errors
static b32 render_blockgroup(MiltonState* milton_state,
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
    for ( i32 block_i = 0; block_i < blocks_per_blockgroup; ++block_i ) {
        if ( block_start + block_i >= num_blocks ) {
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

    // Filter strokes to this blockgroup.
    b32* stroke_masks = filter_strokes_to_rect(blockgroup_arena,
                                               milton_state->strokes,
                                               canvas_blockgroup_rect);



    if (!stroke_masks) {
        allocation_ok = false;
    }

    Arena render_arena = { 0 };
    if ( allocation_ok ) {
        render_arena = arena_spawn(blockgroup_arena, arena_available_space(blockgroup_arena));
    }

    for ( int block_i = 0; block_i < blocks_per_blockgroup && allocation_ok; ++block_i ) {
        if ( block_start + block_i >= num_blocks ) {
            break;
        }

        if ( milton_state->cpu_has_sse2 ) {
            allocation_ok = rasterize_canvas_block_sse2(&render_arena,
                                                        milton_state->view,
                                                        milton_state->strokes,
                                                        stroke_masks,
                                                        &milton_state->working_stroke,
                                                        raster_buffer,
                                                        blocks[block_start + block_i]);
        } else {
            allocation_ok = rasterize_canvas_block_slow(&render_arena,
                                                        milton_state->view,
                                                        milton_state->strokes,
                                                        stroke_masks,
                                                        &milton_state->working_stroke,
                                                        raster_buffer,
                                                        blocks[block_start + block_i]);
        }
        arena_reset(&render_arena);
    }
    return allocation_ok;
}

int render_worker(void* data)
{
    WorkerParams* params = (WorkerParams*) data;
    MiltonState* milton_state = params->milton_state;
    i32 id = params->worker_id;
    RenderQueue* render_queue = milton_state->render_queue;

    for ( ;; ) {
        int err = SDL_SemWait(render_queue->work_available);
        if ( err != 0 ) {
            milton_fatal("Failure obtaining semaphore inside render worker");
        }
        err = SDL_LockMutex(render_queue->mutex);
        if ( err != 0 ) {
            milton_fatal("Failure locking render queue mutex");
        }
        i32 index = -1;
        BlockgroupRenderData blockgroup_data = { 0 };
        {
            index = --render_queue->index;
            assert (index >= 0);
            assert (index <  RENDER_QUEUE_SIZE);
            blockgroup_data = render_queue->blockgroup_render_data[index];
            SDL_UnlockMutex(render_queue->mutex);
        }

        assert (index >= 0);

        b32 allocation_ok = render_blockgroup(milton_state,
                                              &milton_state->render_worker_arenas[id],
                                              render_queue->blocks,
                                              blockgroup_data.block_start, render_queue->num_blocks,
                                              render_queue->raster_buffer);
        if ( !allocation_ok ) {
            milton_state->worker_needs_memory = true;
        }

        arena_reset(&milton_state->render_worker_arenas[id]);

        SDL_SemPost(render_queue->completed_semaphore);
    }
}

static void produce_render_work(MiltonState* milton_state,
                                BlockgroupRenderData blockgroup_render_data)
{
    RenderQueue* render_queue = milton_state->render_queue;
    i32 lock_err = SDL_LockMutex(milton_state->render_queue->mutex);
    if ( !lock_err ) {
        render_queue->blockgroup_render_data[render_queue->index++] = blockgroup_render_data;
        SDL_UnlockMutex(render_queue->mutex);
    }
    else {
        assert (!"Lock failure not handled");
    }

    SDL_SemPost(render_queue->work_available);
}


// Returns true if operation was completed.
static void render_canvas(MiltonState* milton_state, u32* raster_buffer, Rect raster_limits)
{
    Rect* blocks = NULL;

    i32 num_blocks = rect_split(milton_state->transient_arena,
                                raster_limits,
                                milton_state->block_width, milton_state->block_width, &blocks);

    if ( num_blocks < 0 ) {
        milton_log ("[ERROR] Transient arena did not have enough memory for canvas block.\n");
        milton_fatal("Not handling this error.");
    }

    RenderQueue* render_queue = milton_state->render_queue;
    {
        render_queue->blocks = blocks;
        render_queue->num_blocks = num_blocks;
        render_queue->raster_buffer = raster_buffer;
    }

    const i32 blocks_per_blockgroup = milton_state->blocks_per_blockgroup;

    i32 blockgroup_acc = 0;
    for ( int block_i = 0; block_i < num_blocks; block_i += blocks_per_blockgroup ) {
        if ( block_i >= num_blocks ) {
            break;
        }

#define RENDER_MULTITHREADED 1
#if RENDER_MULTITHREADED
        BlockgroupRenderData data =
        {
            .block_start = block_i,
        };

        produce_render_work(milton_state, data);
        blockgroup_acc += 1;
#else
        Arena blockgroup_arena = arena_push(milton_state->transient_arena,
                                            arena_available_space(milton_state->transient_arena));
        render_blockgroup(milton_state,
                          &blockgroup_arena,
                          blocks,
                          block_i, num_blocks,
                          raster_buffer);

        arena_pop(&blockgroup_arena);
#endif
        ARENA_VALIDATE(milton_state->transient_arena);
    }

#if RENDER_MULTITHREADED
    // Wait for workers to finish.

    while(blockgroup_acc) {
        i32 waited_err = SDL_SemWait(milton_state->render_queue->completed_semaphore);
        if (!waited_err) {
            --blockgroup_acc;
        }
        else { assert ( !"Not handling completion semaphore wait error" ); }
    }
#endif

    ARENA_VALIDATE(milton_state->transient_arena);
}

static void render_picker(ColorPicker* picker, u32* buffer_pixels, CanvasView* view)
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
    for ( int j = draw_rect.top; j < draw_rect.bottom; ++j ) {
        for ( int i = draw_rect.left; i < draw_rect.right; ++i ) {
            u32 picker_i =
                    (j - draw_rect.top) * (2*picker->bounds_radius_px ) + (i - draw_rect.left);
            u32 src = buffer_pixels[j * view->screen_size.w + i];
            picker->pixels[picker_i] = src;
        }
    }

    // Render background color.
    for ( int j = draw_rect.top; j < draw_rect.bottom; ++j ) {
        for ( int i = draw_rect.left; i < draw_rect.right; ++i ) {
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
    for ( int j = draw_rect.top; j < draw_rect.bottom; ++j ) {
        for ( int i = draw_rect.left; i < draw_rect.right; ++i ) {
            u32 linear_color = *to_blit++;
            v4f rgba = color_u32_to_v4f(linear_color);
            u32 color = color_v4f_to_u32(rgba);

            buffer_pixels[j * view->screen_size.w + i] = color;
        }
    }
}

static void blit_bitmap(u32* raster_buffer, i32 raster_buffer_width, i32 raster_buffer_height,
                        i32 x, i32 y, Bitmap* bitmap)
{

    if (!bitmap->data) {
        // Fail silently when bitmap is NULL. Will happen until the Git situation is resolved...
        return;
    }
    Rect limits = {
        .left = max(x, 0),
        .top = max(y, 0),
        .right = min(raster_buffer_width, x + bitmap->width),
        .bottom = min(raster_buffer_height, y + bitmap->height),
    };

    if (bitmap->num_components != 4) {
        assert (bitmap->num_components && !"not implemented");
    }

    u32* src_data = (u32*)bitmap->data;
    for ( i32 j = limits.top; j < limits.bottom; ++j ) {
        for ( i32 i = limits.left; i < limits.right; ++i ) {
            i32 index = j * raster_buffer_width + i;
            raster_buffer[index] = *src_data++;
        }
    }

}

static void render_gui_button(u32* raster_buffer, i32 w, i32 h, GuiButton* button)
{
    // TODO: If assets are not present. Draw a flat rectangle.
    blit_bitmap(raster_buffer, w, h, 10, 300, &button->bitmap);
}

static void render_gui(MiltonState* milton_state,
                     u32* raster_buffer, Rect raster_limits,
                     MiltonRenderFlags render_flags)
{
    b32 redraw = false;
    Rect picker_rect = rect_union(picker_get_bounds(&milton_state->gui->picker),
                                  picker_color_buttons_bounds(&milton_state->gui->picker));
    Rect clipped = rect_intersect(picker_rect, raster_limits);
    if ( (clipped.left != clipped.right) && clipped.top != clipped.bottom ) {
        redraw = true;
    }
    MiltonGui* gui = milton_state->gui;
    if ( redraw || (render_flags & MiltonRenderFlags_PICKER_UPDATED) ) {
        render_canvas(milton_state, raster_buffer, picker_rect);

        render_picker(&milton_state->gui->picker,
                      raster_buffer,
                      milton_state->view);

        // Render history buttons for picker
        ColorButton* button = &milton_state->gui->picker.color_buttons;
        while(button) {
            if (button->color.a == 0) {
                break;
            }
            draw_rectangle_with_margin(raster_buffer,
                                       milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                                       button->center_x, button->center_y,
                                       button->width, button->height,
                                       button->color,
                                       // Black margin
                                       (v4f){ 0, 0, 0, 1 });
            button = button->next;
        }

        // Draw an outlined circle for selected color.
        i32 circle_radius = 20;
        i32 circle_shift_left = 20;
        i32 picker_radius = gui->picker.bounds_radius_px;
        i32 ring_girth = 1;
        i32 center_shift = picker_radius - circle_radius - 2*ring_girth;
        i32 x = gui->picker.center.x + center_shift;
        i32 y = gui->picker.center.y - center_shift;
        draw_circle(raster_buffer,
                    milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                    x, y,
                    circle_radius,
                    color_rgb_to_rgba(hsv_to_rgb(gui->picker.info.hsv), 1.0f));
        draw_ring(raster_buffer,
                  milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                  x, y,
                  circle_radius, ring_girth,
                  (v4f) { 0 });
    }


    // Render button
    render_gui_button(raster_buffer,
                      milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                      &gui->brush_button);
}

void milton_render(MiltonState* milton_state, MiltonRenderFlags render_flags)
{
    // `raster_limits` is the part of the screen (in pixels) that should be updated
    // with what's on the canvas.
    Rect raster_limits = { 0 };

    // Figure out what `raster_limits` should be.
    {
        if ( render_flags & MiltonRenderFlags_FULL_REDRAW ) {
            raster_limits.left = 0;
            raster_limits.right = milton_state->view->screen_size.w;
            raster_limits.top = 0;
            raster_limits.bottom = milton_state->view->screen_size.h;
            raster_limits = rect_stretch(raster_limits, milton_state->block_width);
        } else if ( milton_state->working_stroke.num_points > 1 ) {
            Stroke* stroke = &milton_state->working_stroke;

            raster_limits = bounding_box_for_last_n_points(stroke, 20);
            raster_limits = canvas_rect_to_raster_rect(milton_state->view, raster_limits);
            raster_limits = rect_stretch(raster_limits, milton_state->block_width);
            raster_limits = rect_stretch(raster_limits, milton_state->block_width);
            raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);

        } else if ( milton_state->working_stroke.num_points == 1 ) {
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

        if ( render_flags & MiltonRenderFlags_FINISHED_STROKE ) {
            i32 index = milton_state->strokes->count - 1;
            Rect canvas_rect = bounding_box_for_last_n_points(StrokeCord_get(milton_state->strokes,
                                                                              index),
                                                              4);
            raster_limits = canvas_rect_to_raster_rect(milton_state->view, canvas_rect);
            raster_limits = rect_stretch(raster_limits, milton_state->block_width);
            raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);
        }
    }
    VALIDATE_RECT(raster_limits);

    u32* raster_buffer = (u32*)milton_state->raster_buffer;

#if 0
    // DEBUG always render the whole thing.
    {
        raster_limits.left = 0;
        raster_limits.right = milton_state->view->screen_size.w;
        raster_limits.top = 0;
        raster_limits.bottom = milton_state->view->screen_size.h;
    }
#endif

    render_canvas(milton_state, raster_buffer, raster_limits);

    render_gui(milton_state, raster_buffer, raster_limits, render_flags);

    i32 w = milton_state->view->screen_size.w;
    i32 h = milton_state->view->screen_size.h;
}
