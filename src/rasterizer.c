// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.


#include "canvas.h"
#include "color.h"
#include "gui.h"
#include "milton.h"
#include "platform.h"
#include "profiler.h"

// Special value for num_points member of `ClippedStroke` to indicate that the
// block should be flood-filled
#define CLIPPED_STROKE_FILLS_BLOCK 0

// Always accessed together. Keeping them sequential to save some cache misses.
typedef struct ClippedPoint_s {
    i32 x;
    i32 y;
    f32 pressure;
} ClippedPoint;

typedef struct ClippedStroke_s ClippedStroke;

struct ClippedStroke_s {
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

    // A clipped stroke is never empty. We define CLIPPED_STROKE_FILLS_BLOCK to
    // 0 to denote a stroke that fills the block completely, allowing us to go
    // through the "fast path" of just flood-filling the block, which is a
    // common occurrence.
    i32             num_points;

    // Point data for segments AB BC CD DE etc..
    // Yes, it is a linked list. Shoot me.
    //
    // Cache misses are not a problem in the stroke rasterization path.
    ClippedStroke*  next;
};


static b32 clipped_stroke_fills_block(ClippedStroke* clipped_stroke)
{
    b32 fills = clipped_stroke->num_points == CLIPPED_STROKE_FILLS_BLOCK;
    return fills;
}

// Clipped stroke points are stored relative to the reference point.
static ClippedStroke* stroke_clip_to_rect(Arena* render_arena, Stroke* in_stroke,
                                          Rect canvas_rect, i32 local_scale, v2i reference_point)
{
    ClippedStroke* clipped_stroke = arena_alloc_elem(render_arena, ClippedStroke);
    if (!clipped_stroke) {
        return NULL;
    }

    clipped_stroke->next = NULL;
    clipped_stroke->brush = in_stroke->brush;

    // ... now substitute the point data with an array of our own.
    if (in_stroke->num_points > 0) {
        clipped_stroke->num_points = 0;

        // Add enough zeroed-out points to align the arrays to a multiple of 4
        // points so that the renderer can comfortably load from the arrays
        // without bounds checking.
        i32 points_allocated = in_stroke->num_points * 2 + 4;

        clipped_stroke->clipped_points = arena_alloc_array(render_arena, points_allocated, ClippedPoint);
    } else {
        milton_log("WARNING: An empty stroke was received to clip.\n");
        return clipped_stroke;
    }

    if ( !clipped_stroke->clipped_points) {
        // We need more memory. Return gracefully
        return NULL;
    }
    if ( in_stroke->num_points == 1 ) {
        if (is_inside_rect(canvas_rect, in_stroke->points[0])) {
            clipped_stroke->num_points = 1;
            clipped_stroke->clipped_points[0].x = in_stroke->points[0].x * local_scale - reference_point.x;
            clipped_stroke->clipped_points[0].y = in_stroke->points[0].y * local_scale - reference_point.y;
            clipped_stroke->clipped_points[0].pressure = in_stroke->pressures[0];
        }
    } else if ( in_stroke->num_points > 1 ){
        i32 num_points = in_stroke->num_points;
        for (i32 point_i = 0; point_i < num_points - 1; ++point_i) {
            v2i a = in_stroke->points[point_i];
            v2i b = in_stroke->points[point_i + 1];

            f32 pressure_a = in_stroke->pressures[point_i];
            f32 pressure_b = in_stroke->pressures[point_i + 1];

            // Very conservative...
            b32 maybe_inside = !((a.x > canvas_rect.right  && b.x > canvas_rect.right) ||
                                 (a.x < canvas_rect.left   && b.x < canvas_rect.left) ||
                                 (a.y < canvas_rect.top    && b.y < canvas_rect.top) ||
                                 (a.y > canvas_rect.bottom && b.y > canvas_rect.bottom));

            // We can add the segment
            if ( maybe_inside ) {
                clipped_stroke->clipped_points[clipped_stroke->num_points].x     = a.x * local_scale - reference_point.x;
                clipped_stroke->clipped_points[clipped_stroke->num_points].y     = a.y * local_scale - reference_point.y;
                clipped_stroke->clipped_points[clipped_stroke->num_points + 1].x = b.x * local_scale - reference_point.x;
                clipped_stroke->clipped_points[clipped_stroke->num_points + 1].y = b.y * local_scale - reference_point.y;

                clipped_stroke->clipped_points[clipped_stroke->num_points].pressure     = pressure_a;
                clipped_stroke->clipped_points[clipped_stroke->num_points + 1].pressure = pressure_b;

                clipped_stroke->num_points += 2;
            }
        }
    } else {
        // We should have already handled the pathological case of the empty stroke.
        assert (!"invalid code path");
    }
    return clipped_stroke;
}

static b32 is_rect_filled_by_stroke(Rect rect, i32 local_scale, v2i reference_point,
                                    ClippedPoint* points,
                                    i32 num_points,
                                    Brush brush, CanvasView* view)
{
    assert ((((rect.left + rect.right) / 2) * local_scale - reference_point.x) == 0);
    assert ((((rect.top + rect.bottom) / 2) * local_scale - reference_point.y) == 0);
#if 0
    // Perf note: With the current use, this is actually going to be zero.
    v2i rect_center =
    {
        ((rect.left + rect.right) / 2) * local_scale - reference_point.x,
        ((rect.top + rect.bottom) / 2) * local_scale - reference_point.y,
    };
    assert (rect_center.x == 0 && rect_center.y == 0);
#endif

    // We need to move the rect to "local coordinates" specified by local scale and reference point.
    f32 left   = (f32)rect.left   * local_scale - reference_point.x;
    f32 right  = (f32)rect.right  * local_scale - reference_point.x;
    f32 top    = (f32)rect.top    * local_scale - reference_point.y;
    f32 bottom = (f32)rect.bottom * local_scale - reference_point.y;

    if (num_points >= 2) {
        assert (num_points % 2 == 0);
        for ( i32 point_i = 0; point_i < num_points; point_i += 2 ) {
            i32 ax  = points[point_i].x;
            i32 ay  = points[point_i].y;
            i32 bx  = points[point_i + 1].x;
            i32 by  = points[point_i + 1].y;
            f32 p_a = points[point_i].pressure;
            f32 p_b = points[point_i + 1].pressure;

            // Get closest point
            v2f ab = {(f32)(bx - ax), (f32)(by - ay)};
            f32 mag_ab2 = ab.x * ab.x + ab.y * ab.y;

            // Note (v2i){0} is the center of the rect.
            v2f p  = closest_point_in_segment_f(ax, ay, bx, by, ab, mag_ab2, (v2i){0}, NULL);

            f32 pressure = min(p_a, p_b);

            // Half width of a rectangle contained by brush at point p.
            f32 rad = brush.radius * local_scale * 0.707106781f * pressure;  // cos(pi/4)

            f32 b_left = p.x - rad;
            f32 b_right = p.x + rad;
            f32 b_top = p.y - rad;
            f32 b_bottom = p.y + rad;

            // Rect within rect
            if ((left >= b_left) &&
                (right <= b_right) &&
                (top >= b_top) &&
                (bottom <= b_bottom)) {
                return true;
            }
        }
    } else if ( num_points == 1 ) {
        // Half width of a rectangle contained by brush at point p.
        f32 rad = brush.radius * local_scale * 0.707106781f * points[0].pressure;  // cos(pi/4)
        i32 p_x = points[0].x;
        i32 p_y = points[0].y;
        f32 b_left   = p_x - rad;
        f32 b_right  = p_x + rad;
        f32 b_top    = p_y - rad;
        f32 b_bottom = p_y + rad;

        // Rect within rect
        if ((left >= b_left) &&
            (right <= b_right) &&
            (top >= b_top) &&
            (bottom <= b_bottom)) {
            return true;
        }
    }
    return false;
}

static b32 stroke_is_not_tiny(Stroke* stroke, CanvasView* view)
{
    b32 not_tiny = false;
    i32 threshold_px = 4;
    i32 threshold_px_rt = 2;  // the sqrt, for checking distance instead of area..

    // Do a quick early check. If the first & last points are far apart, it is not tiny.
    {
        v2i first = canvas_to_raster(view, stroke->points[0]);
        v2i last = canvas_to_raster(view, stroke->points[stroke->num_points - 1]);
        i32 dist = (i32)manhattan_distance(first, last);
        if ( dist > threshold_px_rt ) {
            not_tiny = true;
        }
    }

    if ( !not_tiny ) {
        Rect bounds = {
            .left = 1 << 30,
            .right = -1<<30,
            .top = 1<<30,
            .bottom = -1<<30,
        };

        for ( i32 i = 0; i < stroke->num_points; ++i ) {
            v2i p = stroke->points[i];
            if ( p.x < bounds.left ) bounds.left = p.x;
            if ( p.y < bounds.top ) bounds.top = p.y;
            if ( p.x > bounds.right ) bounds.right = p.x;
            if ( p.y > bounds.bottom ) bounds.bottom = p.y;
        }

        bounds.top_left = canvas_to_raster(view, bounds.top_left);
        bounds.bot_right = canvas_to_raster(view, bounds.bot_right);

        not_tiny = rect_area(bounds) > threshold_px;
    }

    return not_tiny;
}

// Fills a linked list of strokes that this block needs to render.
// Returns true if allocation succeeded, false if not.
static ClippedStroke* clip_strokes_to_block(Arena* render_arena,
                                            CanvasView* view,
                                            Stroke* strokes,
                                            b32* stroke_masks,
                                            Stroke* working_stroke,
                                            Rect canvas_block,
                                            i32 local_scale,
                                            v2i reference_point,
                                            b32* allocation_ok)
{
    ClippedStroke* stroke_list = NULL;
    *allocation_ok = true;
    size_t num_strokes = sb_count(strokes);

    // Fill linked list with strokes clipped to this block
    for ( size_t stroke_i = 0; stroke_i <= num_strokes; ++stroke_i ) {
        if ( stroke_i < num_strokes && !stroke_masks[stroke_i] ) {
            // Stroke masks is of size num_strokes, but we use stroke_i ==
            // num_strokes to indicate the current "working stroke"
            continue;
        }
        Stroke* unclipped_stroke = NULL;
        if ( stroke_i == num_strokes ) {
            if ( working_stroke->num_points ) {
                unclipped_stroke = working_stroke;
            } else {
                break;
            }
        } else {
            unclipped_stroke = &strokes[stroke_i];
        }
        assert(unclipped_stroke);
        if ( stroke_is_not_tiny(unclipped_stroke, view) ) {
            Rect enlarged_block = rect_enlarge(canvas_block, unclipped_stroke->brush.radius);
            ClippedStroke* clipped_stroke = stroke_clip_to_rect(render_arena, unclipped_stroke,
                                                                enlarged_block, local_scale, reference_point);
            // ALlocation failed.
            // Handle this gracefully; this will cause more memory for render workers.
            if ( !clipped_stroke ) {
                *allocation_ok = false;
                return NULL;
            }

            if ( clipped_stroke->num_points ) {
                // Empty strokes ignored.
                ClippedStroke* list_head = clipped_stroke;

                list_head->next = stroke_list;
                if ( is_rect_filled_by_stroke(canvas_block, local_scale, reference_point,
                                              clipped_stroke->clipped_points,
                                              clipped_stroke->num_points,
                                              clipped_stroke->brush,
                                              view) ) {
                    // Set num_points = 0. Since we are ignoring empty strokes
                    // (which should not get here in the first place), we can use 0
                    // to denote a stroke that fills the block, saving ourselves a
                    // boolean struct member.
                    list_head->num_points = CLIPPED_STROKE_FILLS_BLOCK;
                }
                stroke_list = list_head;
            }
        }
    }

    // Set our `stroke_list` to end at the first opaque stroke that fills
    // this block.
    ClippedStroke* list_iter = stroke_list;
    while (list_iter) {
        if ( clipped_stroke_fills_block(list_iter) && list_iter->brush.color.a == 1.0f ) {
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
                                       Stroke* strokes,
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
    i32 local_scale =  (view->scale <= 8) ?  4 : 1;
    {
        reference_point.x *= local_scale;
        reference_point.y *= local_scale;
    }
    ClippedStroke* stroke_list = clip_strokes_to_block(render_arena, view,
                                                       strokes, stroke_masks, working_stroke,
                                                       canvas_block, local_scale, reference_point,
                                                       &allocation_ok);
    if (!allocation_ok) {
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
          pixel_j += downsample_factor ) {
        i32 i =  (((raster_block.left - view->screen_center.x) *
                    view->scale) - view->pan_vector.x) * local_scale - reference_point.x;

        for ( i32 pixel_i = raster_block.left;
              pixel_i < raster_block.right;
              pixel_i += downsample_factor ) {
            // Clear color
            v4f background_color;
            background_color.rgb = view->background_color;
            background_color.a = 1.0f;

            // Cumulative blending
            v4f acc_color = { 0 };

            ClippedStroke* list_iter = stroke_list;

            while(list_iter) {
                ClippedStroke* clipped_stroke = list_iter;
                list_iter = list_iter->next;
                b32 is_eraser = (equ4f(clipped_stroke->brush.color, (v4f){ -1, -1, -1, -1 }));

                // Fast path.
                if ( clipped_stroke_fills_block(clipped_stroke) ) {
#if 0 // Visualize it with black
                    v4f dst = {0, 0, 0, is_eraser? 1 : clipped_stroke->brush.color.a};
#else
                    v4f dst = is_eraser? background_color : clipped_stroke->brush.color;
#endif
                    acc_color = blend_v4f(dst, acc_color);
                } else {
                    // Slow path. There are pixels not inside.
                    assert (clipped_stroke->num_points > 0);
                    ClippedPoint* points = clipped_stroke->clipped_points;

                    //v2f min_points[4] = {0};
                    f32 min_dist = FLT_MAX;
                    f32 dx = 0;
                    f32 dy = 0;
                    f32 pressure = 0.0f;

                    if ( clipped_stroke->num_points == 1 ) {
                        dx = (f32)(i - points[0].x);
                        dy = (f32)(j - points[0].y);
                        min_dist = dx * dx + dy * dy;
                        pressure = points[0].pressure;
                    } else {
                        // Find closest point.
                        for (int point_i = 0; point_i < clipped_stroke->num_points - 1; point_i += 2) {
                            i32 ax = points[point_i].x;
                            i32 ay = points[point_i].y;
                            i32 bx = points[point_i + 1].x;
                            i32 by = points[point_i + 1].y;
                            f32 p_a = points[point_i    ].pressure;
                            f32 p_b = points[point_i + 1].pressure;

                            v2f ab = {(f32)(bx - ax), (f32)(by - ay)};
                            f32 mag_ab2 = ab.x * ab.x + ab.y * ab.y;
                            if ( mag_ab2 > 0 ) {
                                f32 t;
                                v2f point = closest_point_in_segment_f(ax, ay, bx, by,
                                                                       ab, mag_ab2,
                                                                       (v2i){i,j}, &t);
                                f32 test_dx = (f32) (i - point.x);
                                f32 test_dy = (f32) (j - point.y);
                                f32 dist = sqrtf(test_dx * test_dx + test_dy * test_dy);
                                f32 test_pressure = (1 - t) * p_a + t * p_b;
                                dist = dist - test_pressure * clipped_stroke->brush.radius * local_scale;
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

                            v4f dst = is_eraser? background_color : clipped_stroke->brush.color;
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
#if FAST_GAMMA
            acc_color.rgb = linear_to_square(acc_color.rgb);
#else
            acc_color.rgb = linear_to_sRGB(acc_color.rgb);
#endif
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
                                       Stroke* strokes,
                                       b32* stroke_masks,
                                       Stroke* working_stroke,
                                       u32* pixels,
                                       Rect raster_block)
{
    PROFILE_BEGIN(sse2);
    PROFILE_BEGIN(preamble);

    __m128 one4 = _mm_set_ps1(1);
    __m128 zero4 = _mm_set_ps1(0);


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
    i32 local_scale =  (view->scale <= 8) ?  4 : 1;

    __m128 local_scale4 = _mm_set_ps1((float)local_scale);
    {
        reference_point.x *= local_scale;
        reference_point.y *= local_scale;
    }
    ClippedStroke* stroke_list = clip_strokes_to_block(render_arena, view,
                                                       strokes, stroke_masks,
                                                       working_stroke,
                                                       canvas_block, local_scale, reference_point,
                                                       &allocation_ok);
    if ( !allocation_ok ) {
        // Request more memory
        return false;
    }

    i32 downsample_factor = view->downsampling_factor;

    // This is the distance between two adjacent canvas pixels. Derived to
    // avoid expensive raster_to_canvas computations in the inner loop
    i32 canvas_jump = downsample_factor * view->scale * local_scale;


    PROFILE_PUSH(preamble);
    PROFILE_BEGIN(total_work_loop);

    // Clear color
    v4f background_color;
    background_color.rgb = view->background_color;
    background_color.a = 1.0f;


    // i and j are the canvas point
    i32 j = (((raster_block.top - view->screen_center.y) *
              view->scale) - view->pan_vector.y) * local_scale - reference_point.y;

    for ( i32 pixel_j = raster_block.top;
          pixel_j < raster_block.bottom;
          pixel_j += downsample_factor ) {
        i32 i = (((raster_block.left - view->screen_center.x) *
                  view->scale) - view->pan_vector.x) * local_scale - reference_point.x;

        for ( i32 pixel_i = raster_block.left;
              pixel_i < raster_block.right;
              pixel_i += downsample_factor ) {
            // Cumulative blending
            v4f acc_color = { 0 };

            ClippedStroke* list_iter = stroke_list;

            while( list_iter ) {
                ClippedStroke* clipped_stroke = list_iter;
                list_iter = list_iter->next;
                b32 is_eraser = equ4f(clipped_stroke->brush.color, (v4f){ -1, -1, -1, -1 });

                // Fast path.
                if ( clipped_stroke_fills_block(clipped_stroke) ) {
#if 0 // Visualize it with black
                    v4f dst = {0, 0, 0, is_eraser? 1 : clipped_stroke->brush.color.a};
#else
                    v4f dst = is_eraser? background_color : clipped_stroke->brush.color;
#endif
                    acc_color = blend_v4f(dst, acc_color);
                } else {
                    assert(clipped_stroke->num_points > 0);
                    // Slow path. There are pixels not inside.
                    ClippedPoint* points = clipped_stroke->clipped_points;

                    f32 min_dist = FLT_MAX;
                    f32 dx = 0;
                    f32 dy = 0;
                    f32 pressure = 0.0f;

                    if ( clipped_stroke->num_points == 1 ) {
                        dx = (f32)(i - points[0].x);
                        dy = (f32)(j - points[0].y);
                        min_dist = dx * dx + dy * dy;
                        pressure = points[0].pressure;
                    } else {
//#define SSE_M(wide, i) ((f32 *)&(wide) + i)
                        for (int point_i = 0; point_i < clipped_stroke->num_points - 1; point_i += (2 * 4) ) {
                            // The step is 4 (128 bit SIMD) times 2 (points are in format AB BC CD DE)

                            PROFILE_BEGIN(load);

                            ALIGN(16) f32 axs[4];
                            ALIGN(16) f32 ays[4];
                            ALIGN(16) f32 bxs[4];
                            ALIGN(16) f32 bys[4];
                            ALIGN(16) f32 aps[4];
                            ALIGN(16) f32 bps[4];

                            // NOTE: This loop is not stupid:
                            //  I transformed the data representation to SOA
                            //  form. There was no measureable difference even
                            //  though this loop turned into 4 _mm_load_ps
                            //  instructions.
                            //
                            // We can comfortably get 4 elements because the
                            // points are allocated with enough trailing zeros.
                            i32 l_point_i = point_i;
                            for ( i32 batch_i = 0; batch_i < 4; batch_i++ ) {
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

                            __m128 a_x = _mm_load_ps(axs);
                            __m128 a_y = _mm_load_ps(ays);
                            __m128 b_x = _mm_load_ps(bxs);
                            __m128 b_y = _mm_load_ps(bys);
                            __m128 pressure_a = _mm_load_ps(aps);
                            __m128 pressure_b = _mm_load_ps(bps);

                            PROFILE_PUSH(load);
                            PROFILE_BEGIN(work);

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

                            PROFILE_PUSH(work);
                            PROFILE_BEGIN(gather);

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

                            // Two versions of the "gather" step. A "smart" version (in quotes because there is probably a better, smarter way of doing it) and a naive loop.
                            // the naive loop used to be faster (before
                            // aligning the stack variables o__o ). Now they
                            // are practically the same.
#if 1
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
                            if (dist < min_dist) {
                                int bit  = _mm_movemask_ps(_mm_cmpeq_ps(_mm_set_ps1(dist), dist4));
                                int batch_i = -1;
#ifdef _WIN32
                                _BitScanForward64((DWORD*)&batch_i, bit);
#else  // TODO: in clang&gcc: __builtin_ctz
                                for (int p = 0; p < 4; ++p) {
                                    if ( bit & (1 << p) ) {
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
                            for ( i32 batch_i = 0; batch_i < 4; ++batch_i ) {
                                f32 dist = dists[batch_i];
                                i32 imask = *(i32*)&masks[batch_i];
                                if (dist < min_dist && imask == -1) {
                                    min_dist = dist;
                                    dx = tests_dx[batch_i];
                                    dy = tests_dy[batch_i];
                                    pressure = pressures[batch_i];
                                }
                            }
#endif
                            PROFILE_PUSH(gather);
                        }
                    }

                    PROFILE_BEGIN(sampling);

                    if ( min_dist < FLT_MAX ) {
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

                            v4f dst = is_eraser? background_color : clipped_stroke->brush.color;
                            {
                                dst.r *= coverage;
                                dst.g *= coverage;
                                dst.b *= coverage;
                                dst.a *= coverage;
                            }

                            acc_color = blend_v4f(dst, acc_color);
                        }
                    }
                    PROFILE_PUSH(sampling);
                }
                if ( acc_color.a > 0.999999 ) {
                    break;
                }
            }

            // Blend onto the background whatever is accumulated.
            acc_color = blend_v4f(background_color, acc_color);

            // Brushes are stored and operated in linear space, move to srgb
            // before blitting
#if FAST_GAMMA
            acc_color.rgb = linear_to_square(acc_color.rgb);
#else
            acc_color.rgb = linear_to_sRGB(acc_color.rgb);
#endif
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
    PROFILE_PUSH(total_work_loop);
    PROFILE_PUSH(sse2);
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
    i32 left   = max(center_x - radius, 0);
    i32 right  = min(center_x + radius, raster_buffer_width);
    i32 top    = max(center_y - radius, 0);
    i32 bottom = min(center_y + radius, raster_buffer_height);

    assert (right >= left);
    assert (bottom >= top);

    for ( i32 j = top; j < bottom; ++j ) {
        for ( i32 i = left; i < right; ++i ) {
            i32 index = j * raster_buffer_width + i;


            //TODO: AA
            f32 dist = distance((v2f){ (f32)i, (f32)j },
                                (v2f){ (f32)center_x, (f32)center_y });

            if (dist < radius) {
                //u32 dst_color = color_v4f_to_u32(to_premultiplied(color_u32_to_v4f(raster_buffer[index]).rgb, 1.0f));
                u32 dst_color = raster_buffer[index];
                v4f blended = blend_v4f(color_u32_to_v4f(dst_color), src_color);
                u32 color = color_v4f_to_u32(blended);
                raster_buffer[index] = color;
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


    for ( i32 j = top; j < bottom; ++j ) {
        for ( i32 i = left; i <= left + margin_px; ++i ) {
            BLEND_HERE();
        }
        for ( i32 i = right - margin_px; i <= right; ++i ) {
            BLEND_HERE();
        }
    }

    for ( i32 i = left; i < right; ++i ) {
        for ( i32 j = top; j <= top + margin_px; ++j) {
            BLEND_HERE();
        }
        for ( i32 j = bottom - margin_px; j <= bottom; ++j ) {
            BLEND_HERE();
        }
    }

#undef BLEND_AT_INDEX
}


static void rasterize_color_picker(ColorPicker* picker,
                                   Rect draw_rect)
{
    // Wheel
    for ( int j = draw_rect.top; j < draw_rect.bottom; ++j ) {
        for ( int i = draw_rect.left; i < draw_rect.right; ++i ) {
            u32 picker_i = (u32) (j - draw_rect.top) *( 2*picker->bounds_radius_px ) + (i - draw_rect.left);
            v2f point = {(f32)i, (f32)j};
            u32 dest_color = picker->pixels[picker_i];

            int samples = 0;
            {
                f32 u = 0.223607f;
                f32 v = 0.670820f;

                samples += (int)picker_hits_wheel(picker, add2f(point, (v2f){-u, -v}));
                samples += (int)picker_hits_wheel(picker, add2f(point, (v2f){-v, u}));
                samples += (int)picker_hits_wheel(picker, add2f(point, (v2f){u, v}));
                samples += (int)picker_hits_wheel(picker, add2f(point, (v2f){v, u}));
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
            u32 picker_i = (u32) (j - draw_rect.top) *( 2*picker->bounds_radius_px ) + (i - draw_rect.left);
            u32 dest_color = picker->pixels[picker_i];
            // MSAA!!
            int samples = 0;
            {
                f32 u = 0.223607f;
                f32 v = 0.670820f;

                samples += (int)is_inside_triangle(add2f(point, (v2f){-u, -v}), picker->info.a, picker->info.b, picker->info.c);
                samples += (int)is_inside_triangle(add2f(point, (v2f){-v, u}), picker->info.a, picker->info.b, picker->info.c);
                samples += (int)is_inside_triangle(add2f(point, (v2f){u, v}), picker->info.a, picker->info.b, picker->info.c);
                samples += (int)is_inside_triangle(add2f(point, (v2f){v, u}), picker->info.a, picker->info.b, picker->info.c);
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

        v4f color = (v4f){
            1 - rgb.r,
            1 - rgb.g,
            1 - rgb.b,
            1,
        };

        if ( check_flag(picker->flags, ColorPickerFlags_TRIANGLE_ACTIVE) ) {
            ring_radius = 10;
            ring_girth  = 2;
            color       = (v4f){0};
        }

        // Barycentric to cartesian
        f32 a = hsv.s;
        f32 b = 1 - hsv.v;
        f32 c = 1 - a - b;

        v2f point = add2f(add2f((scale2f(picker->info.c,a)), scale2f(picker->info.b,b)), scale2f(picker->info.a,c));

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

        if ( SDL_HasSSE2() && !milton_state->DEBUG_sse2_switch ) {
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

int renderer_worker_thread(void* data)
{
    WorkerParams* params = (WorkerParams*) data;
    MiltonState* milton_state = params->milton_state;
    i32 id = params->worker_id;
    RenderStack* render_stack = milton_state->render_stack;

    for ( ;; ) {
        int err = SDL_SemWait(render_stack->work_available);
        if ( err != 0 ) {
            milton_fatal("Failure obtaining semaphore inside render worker");
        }
        err = SDL_LockMutex(render_stack->mutex);
        if ( err != 0 ) {
            milton_fatal("Failure locking render queue mutex");
        }
        i32 index = -1;
        BlockgroupRenderData blockgroup_data = { 0 };
        {
            index = --render_stack->index;
            assert (index >= 0);
            assert (index <  RENDER_STACK_SIZE);
            blockgroup_data = render_stack->blockgroup_render_data[index];
            SDL_UnlockMutex(render_stack->mutex);
        }

        assert (index >= 0);

        b32 allocation_ok = render_blockgroup(milton_state,
                                              &milton_state->render_worker_arenas[id],
                                              render_stack->blocks,
                                              blockgroup_data.block_start, render_stack->num_blocks,
                                              render_stack->canvas_buffer);
        if ( !allocation_ok ) {
            milton_state->worker_needs_memory = true;
        }

        arena_reset(&milton_state->render_worker_arenas[id]);

        SDL_SemPost(render_stack->completed_semaphore);
    }
}

#if MILTON_MULTITHREADED
static void produce_render_work(MiltonState* milton_state,
                                BlockgroupRenderData blockgroup_render_data)
{
    RenderStack* render_stack = milton_state->render_stack;
    i32 lock_err = SDL_LockMutex(milton_state->render_stack->mutex);
    if ( !lock_err ) {
        if ( render_stack->index < RENDER_STACK_SIZE ) {
            render_stack->blockgroup_render_data[render_stack->index++] = blockgroup_render_data;
        }
        SDL_UnlockMutex(render_stack->mutex);
    }
    else {
        assert (!"Lock failure not handled");
    }

    SDL_SemPost(render_stack->work_available);
}
#endif

static void render_canvas(MiltonState* milton_state, Rect raster_limits)
{
    PROFILE_BEGIN(render_canvas);

    Rect* blocks = NULL;
    i32 num_blocks = rect_split(&blocks, raster_limits, milton_state->block_width, milton_state->block_width);

    if ( num_blocks > 0 ) assert(blocks != NULL);

    if ( num_blocks < 0 ) {
        milton_log ("[ERROR] Transient arena did not have enough memory for canvas block.\n");
        milton_fatal("Not handling this error.");
    }

    RenderStack* render_stack = milton_state->render_stack;
    {
        render_stack->blocks = blocks;
        render_stack->num_blocks = (i32)sb_count(blocks);
        render_stack->canvas_buffer = (u32*)milton_state->canvas_buffer;
    }

    const i32 blocks_per_blockgroup = milton_state->blocks_per_blockgroup;

    i32 blockgroup_acc = 0;
    for ( int block_i = 0; block_i < num_blocks; block_i += blocks_per_blockgroup ) {
        if ( block_i >= num_blocks ) {
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
                          &blockgroup_arena,
                          blocks,
                          block_i, num_blocks,
                          (u32*)milton_state->canvas_buffer);

        arena_pop(&blockgroup_arena);
#endif
    }

#if MILTON_MULTITHREADED
    // Wait for workers to finish.

    while(blockgroup_acc) {
        i32 waited_err = SDL_SemWait(milton_state->render_stack->completed_semaphore);
        if (!waited_err) {
            --blockgroup_acc;
        }
        else { assert ( !"Not handling completion semaphore wait error" ); }
    }
#endif

    sb_free(blocks);
    PROFILE_PUSH(render_canvas);
}

// Call render_canvas with increasing quality until it is too slow.
static void render_canvas_iteratively(MiltonState* milton_state, Rect raster_limits)
{
    CanvasView* view = milton_state->view;
    i32 original_df = view->downsampling_factor;

    i32 time_ms = 0;
    view->downsampling_factor = 8;  // Starts at 4, the loop divides at the start.

    while ( view->downsampling_factor > 1 && time_ms < 15 ) {
        view->downsampling_factor /= 2;
        i32 start_ms = SDL_GetTicks();
        render_canvas(milton_state, raster_limits);
        time_ms += SDL_GetTicks() - start_ms;
    }

    view->downsampling_factor = original_df;
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
            u32 picker_i = (u32) (j - draw_rect.top) * (2*picker->bounds_radius_px ) + (i - draw_rect.left);
            u32 src = buffer_pixels[j * view->screen_size.w + i];
            picker->pixels[picker_i] = src;
        }
    }

    // Render background color.
    for ( int j = draw_rect.top; j < draw_rect.bottom; ++j ) {
        for ( int i = draw_rect.left; i < draw_rect.right; ++i ) {
            u32 picker_i = (u32) (j - draw_rect.top) *( 2*picker->bounds_radius_px ) + (i - draw_rect.left);


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
    Rect limits = {0};
    limits.left   = max(x, 0);
    limits.top    = max(y, 0);
    limits.right  = min(raster_buffer_width, x + bitmap->width);
    limits.bottom = min(raster_buffer_height, y + bitmap->height);

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
static void copy_canvas_to_raster_buffer(MiltonState* milton_state, Rect rect)
{
    u32* raster_ptr = (u32*)milton_state->raster_buffer;
    u32* canvas_ptr = (u32*)milton_state->canvas_buffer;
    for (i32 j = rect.top; j <= rect.bottom; ++j) {
        for (i32 i = rect.left; i <= rect.right; ++i) {
            i32 bufi = j*milton_state->view->screen_size.w + i;
            raster_ptr[bufi] = canvas_ptr[bufi];
        }
    }
}

static void render_gui_button(u32* raster_buffer, i32 w, i32 h, GuiButton* button)
{
    blit_bitmap(raster_buffer, w, h, 10, 300, &button->bitmap);
}

static void render_gui(MiltonState* milton_state, Rect raster_limits, MiltonRenderFlags render_flags)
{
    const b32 gui_visible = milton_state->gui->visible;  // Some elements are not affected by this, like the hover outline
    b32 redraw = false;
    Rect picker_rect = get_bounds_for_picker_and_colors(&milton_state->gui->picker);
    Rect clipped = rect_intersect(picker_rect, raster_limits);
    if ( (clipped.left != clipped.right) && clipped.top != clipped.bottom ) {
        redraw = true;
    }
    u32* raster_buffer = (u32*)milton_state->raster_buffer;
    MiltonGui* gui = milton_state->gui;
    if ( gui_visible && (redraw || (check_flag(render_flags, MiltonRenderFlags_PICKER_UPDATED))) ) {

        render_picker(&milton_state->gui->picker,
                      raster_buffer,
                      milton_state->view);

        // Render history buttons for picker
        ColorButton* button = &milton_state->gui->picker.color_buttons;
        while(button) {
            if (button->color.a == 0) {
                break;
            }
            fill_rectangle_with_margin(raster_buffer,
                                       milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                                       button->x, button->y,
                                       button->w, button->h,
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
                  (v4f){0});
    }


    if ( gui_visible ) {  // Render button
        render_gui_button(raster_buffer,
                          milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                          &gui->brush_button);

        if (check_flag(render_flags, MiltonRenderFlags_BRUSH_PREVIEW)) {
            assert (gui->preview_pos.x >= 0 && gui->preview_pos.y >= 0);
            const i32 radius = milton_get_brush_size(milton_state);
            {
                i32 r = k_max_brush_size + 2;
                i32 x = gui->preview_pos_prev.x != -1? gui->preview_pos_prev.x : gui->preview_pos.x;
                i32 y = gui->preview_pos_prev.y != -1? gui->preview_pos_prev.y : gui->preview_pos.y;
            }

            v4f preview_color;
            preview_color.rgb = milton_state->view->background_color;
            preview_color.a = 1;
            if ( milton_state->current_mode == MiltonMode_PEN ) {
                preview_color = to_premultiplied(hsv_to_rgb(gui->picker.info.hsv), milton_get_pen_alpha(milton_state));
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
                      (v4f){0});
            gui->preview_pos_prev = gui->preview_pos;
            gui->preview_pos = (v2i){ -1, -1 };
            // TODO: Request redraw rect here.
        }
    }

    // If the explorer is active, render regardless of UI being visible

    if ( milton_state->current_mode == MiltonMode_EXPORTING  ) {
        Exporter* exporter = &gui->exporter;
        if ( exporter->state == ExporterState_GROWING_RECT || exporter->state == ExporterState_SELECTED ) {
            i32 x = min(exporter->pivot.x, exporter->needle.x);
            i32 y = min(exporter->pivot.y, exporter->needle.y);
            i32 w = abs(exporter->pivot.x - exporter->needle.x);
            i32 h = abs(exporter->pivot.y - exporter->needle.y);

            i32 center_x = x + w / 2;
            i32 center_y = y + h / 2;

            rectangle_margin(raster_buffer,
                             milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                             x,y,w,h,
                             (v4f){ 0, 0, 0, 1 }, 1);
        }
    }

    if ( check_flag(render_flags, MiltonRenderFlags_BRUSH_HOVER) ) {
        const i32 radius = milton_get_brush_size(milton_state);
        draw_ring(raster_buffer,
                  milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                  milton_state->hover_point.x, milton_state->hover_point.y,
                  radius, 1,
                  (v4f){0});
    }
}

void milton_render(MiltonState* milton_state, MiltonRenderFlags render_flags, v2i pan_delta)
{
    // `raster_limits` is the part of the screen (in pixels) that should be updated
    // with what's on the canvas.
    Rect raster_limits = { 0 };

    //  - If the screen is panning, copy the part of the buffer we can reuse.
    //  - For whatever reason, we might be asked a full redraw.
    //  - If we don't get a full redraw, then figure out what to draw based on the
    //      current working stroke.

#if MILTON_ENABLE_PROFILING
        set_flag(render_flags, MiltonRenderFlags_FULL_REDRAW);
#endif

    if ( check_flag(render_flags, MiltonRenderFlags_PAN_COPY) ) {
        CanvasView* view = milton_state->view;

        // Copy the canvas buffer to the raster buffer. We will use the raster
        // buffer as temporary storage. It will be rewritten later.
        memcpy(milton_state->raster_buffer, milton_state->canvas_buffer,
               milton_state->bytes_per_pixel * view->screen_size.w * view->screen_size.h);


        // Dimensions of rectangle
        v2i size = {
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

        if ( pan_delta.x > 0 ) {
            vertical.left  = 0;
            vertical.right = pan_delta.x + pad;
        } else {
            vertical.left  = view->screen_size.w + pan_delta.x - pad;
            vertical.right = view->screen_size.w;
        }

        if ( pan_delta.y > 0 ) {
            horizontal.top    = 0;
            horizontal.bottom = pan_delta.y + pad;
        } else if ( pan_delta.y < 0 ) {
            horizontal.top    = view->screen_size.h + pan_delta.y - pad;
            horizontal.bottom = view->screen_size.h;
        }

        // Extend the rects to cover at least one block
        {
            int vw = vertical.right - vertical.left;
            int vh = vertical.bottom - vertical.top;
            int hw = horizontal.right - horizontal.left;
            int hh = horizontal.bottom - horizontal.top;
            if (vw > 0 && vw < pad) {
                vw = pad;
            }
            if (vh > 0 && vh < pad) {
                vw = pad;
            }
            if (hw > 0 && vw < pad) {
                vw = pad;
            }
            if (hh > 0 && hh < pad) {
                vw = pad;
            }
        }


        render_canvas_iteratively(milton_state, horizontal);
        render_canvas_iteratively(milton_state, vertical);

        v2i dst = {0};  // Starting point to copy canvas buffer
        if ( pan_delta.x > 0 ) {
            dst.x = pan_delta.x;
        }
        if ( pan_delta.y > 0 ) {
            dst.y = pan_delta.y;
        }

        v2i src = {0};  // Starting point of source canvas buffer.
        if ( pan_delta.x < 0 ) {
            src.x = -pan_delta.x;
        }
        if ( pan_delta.y < 0 ) {
            src.y = -pan_delta.y;
        }

        i32 bpp = milton_state->bytes_per_pixel;
        u32* pixels_src = ((u32*)milton_state->raster_buffer) + (src.y*view->screen_size.w + src.x);
        u32* pixels_dst = ((u32*)milton_state->canvas_buffer) + (dst.y*view->screen_size.w + dst.x);

        for ( int j = 0; j < size.h; ++j ) {
            for ( int i = 0; i < size.w; ++i ) {
                *pixels_dst++ = *pixels_src++;
            }
            pixels_dst += view->screen_size.w - size.w;
            pixels_src += view->screen_size.w - size.w;
        }

    }

    if ( check_flag(render_flags, MiltonRenderFlags_FULL_REDRAW) ) {
        raster_limits.left = 0;
        raster_limits.right = milton_state->view->screen_size.w;
        raster_limits.top = 0;
        raster_limits.bottom = milton_state->view->screen_size.h;
        raster_limits = rect_stretch(raster_limits, milton_state->block_width);
    }
    else if ( milton_state->working_stroke.num_points > 1 ) {
        Stroke* stroke = &milton_state->working_stroke;

        raster_limits = bounding_box_for_last_n_points(stroke, 20);
        raster_limits = canvas_rect_to_raster_rect(milton_state->view, raster_limits);
        raster_limits = rect_stretch(raster_limits, milton_state->block_width);
        raster_limits = rect_stretch(raster_limits, milton_state->block_width);
        raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);

    }
    else if ( milton_state->working_stroke.num_points == 1 ) {
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
    else if ( check_flag( render_flags, MiltonRenderFlags_FINISHED_STROKE )) {
        size_t index = sb_count(milton_state->strokes) - 1;
        Rect canvas_rect = bounding_box_for_last_n_points(&milton_state->strokes[index], 4);
        raster_limits = canvas_rect_to_raster_rect(milton_state->view, canvas_rect);
        raster_limits = rect_stretch(raster_limits, milton_state->block_width);
        raster_limits = rect_clip_to_screen(raster_limits, milton_state->view->screen_size);
    }

    if (rect_is_valid(raster_limits)) {
        if ( check_flag(render_flags, MiltonRenderFlags_DRAW_ITERATIVELY) ) {
            render_canvas_iteratively(milton_state, raster_limits);
        } else {
            render_canvas(milton_state, raster_limits);
        }

        // Copy the whole thing. Makes gui elements redraw fast.
        // Disabling this would mean keeping track of GUI updates for
        // canvas redrawing.
        raster_limits.left = 0;
        raster_limits.right = milton_state->view->screen_size.w;
        raster_limits.top = 0;
        raster_limits.bottom = milton_state->view->screen_size.h;
        copy_canvas_to_raster_buffer(milton_state, raster_limits);

        render_gui(milton_state, raster_limits, render_flags);
    } else {
        milton_log("WARNING: Tried to render with invalid rect: (l r t b): %d %d %d %d\n",
                   raster_limits.left,
                   raster_limits.right,
                   raster_limits.top,
                   raster_limits.bottom);
    }
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
    v2i pan_delta = sub2i(center, (v2i){x + (w/2), y + (h/2)}) ;

    milton_state->view->pan_vector = add2i(milton_state->view->pan_vector, scale2i(pan_delta, milton_state->view->scale));

    i32 buf_w = w * scale;
    i32 buf_h = h * scale;

    milton_state->canvas_buffer = buffer;
    milton_state->view->screen_size = (v2i){ buf_w, buf_h };
    milton_state->view->screen_center = divide2i(milton_state->view->screen_size, 2);
    if ( scale > 1 ) {
        milton_state->view->scale = (i32)ceill(((f32)milton_state->view->scale / (f32)scale));
    }

    // TODO:
    // When setting this to be a stand-alone library, we will need to change
    // the signature of render_canvas to only take what it needs, to reduce
    // dependencies on implementation details of the app

    Rect raster_limits;
    raster_limits.left   = 0;
    raster_limits.top    = 0;
    raster_limits.right  = buf_w;
    raster_limits.bottom = buf_h;

    // render_canvas will set worker_needs_memory to true if it fails to render.
    while ( render_canvas(milton_state, raster_limits), milton_state->worker_needs_memory ) {
        milton_expand_render_memory(milton_state);
    }

    // Unset
    milton_state->canvas_buffer = saved_buffer;
    *milton_state->view = saved_view;
}
