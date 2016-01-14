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


const v2i canvas_to_raster(CanvasView* view, v2i canvas_point)
{
    v2i raster_point = {
        ((view->pan_vector.x + canvas_point.x) / view->scale) + view->screen_center.x,
        ((view->pan_vector.y + canvas_point.y) / view->scale) + view->screen_center.y,
    };
    return raster_point;
}

const v2i raster_to_canvas(CanvasView* view, v2i raster_point)
{
    v2i canvas_point = {
        ((raster_point.x - view->screen_center.x) * view->scale) - view->pan_vector.x,
        ((raster_point.y - view->screen_center.y) * view->scale) - view->pan_vector.y,
    };

    return canvas_point;
}

// Returns an array of `num_strokes` b32's, masking strokes to the rect.
b32* filter_strokes_to_rect(Arena* arena,
                            StrokeCord strokes,
                            const Rect rect)
{
    size_t num_strokes = count(strokes);
    b32* mask_array = arena_alloc_array(arena, num_strokes, b32);
    if (!mask_array) {
        return NULL;
    }

    for (size_t stroke_i = 0; stroke_i < num_strokes; ++stroke_i) {
        Stroke* stroke = &strokes[stroke_i];
        Rect stroke_rect = rect_enlarge(rect, stroke->brush.radius);
        VALIDATE_RECT(stroke_rect);
        if (stroke->num_points == 1) {
            if (is_inside_rect(stroke_rect, stroke->points[0])) {
                mask_array[stroke_i] = true;
            }
        } else {
            for (size_t point_i = 0; point_i < (size_t)stroke->num_points - 1; ++point_i) {
                v2i a = stroke->points[point_i];
                v2i b = stroke->points[point_i + 1];

                b32 inside = !((a.x > stroke_rect.right && b.x >  stroke_rect.right) ||
                               (a.x < stroke_rect.left && b.x <   stroke_rect.left) ||
                               (a.y < stroke_rect.top && b.y <    stroke_rect.top) ||
                               (a.y > stroke_rect.bottom && b.y > stroke_rect.bottom));

                if (inside) {
                    mask_array[stroke_i] = true;
                    break;
                }
            }
        }
    }
    return mask_array;
}

// Does point p0 with radius r0 contain point p1 with radius r1?
b32 stroke_point_contains_point(v2i p0, i32 r0,
                                v2i p1, i32 r1)
{
    v2i d = p1 - p0;
    // using manhattan distance, less chance of overflow. Still works well enough for this case.
    u32 m = abs(d.x) + abs(d.y) + r1;
    //i32 m = magnitude_i(d) + r1;
    b32 contained = (m < r0);
    return contained;
}

Rect bounding_box_for_stroke(Stroke* stroke)
{
    Rect bb = bounding_rect_for_points(stroke->points, stroke->num_points);
    Rect bb_enlarged = rect_enlarge(bb, stroke->brush.radius);
    return bb_enlarged;
}

Rect bounding_box_for_last_n_points(Stroke* stroke, i32 last_n)
{
    i32 forward = max(stroke->num_points - last_n, 0);
    i32 num_points = min(last_n, stroke->num_points);
    Rect bb = bounding_rect_for_points(stroke->points + forward, num_points);
    Rect bb_enlarged = rect_enlarge(bb, stroke->brush.radius);
    return bb_enlarged;
}

Rect canvas_rect_to_raster_rect(CanvasView* view, Rect canvas_rect)
{
    Rect raster_rect;
    raster_rect.bot_right = canvas_to_raster(view, canvas_rect.bot_right);
    raster_rect.top_left = canvas_to_raster(view, canvas_rect.top_left);
    return raster_rect;
}

struct StrokeCord::StrokeCordChunk {
    Stroke*	     data;
    StrokeCordChunk* next;
};

static StrokeCord::StrokeCordChunk* StrokeCord_internal_alloc_chunk(size_t chunk_size)
{
    auto* result = (StrokeCord::StrokeCordChunk*)mlt_calloc(1, sizeof(StrokeCord::StrokeCordChunk));
    if (result) {
        result->data = (Stroke*)mlt_calloc(chunk_size, sizeof(Stroke));
        if (!result->data) {
            result = NULL;
        }
    }
    return result;
}

StrokeCord::StrokeCord(size_t chunk_size)
{
    StrokeCord* cord = (StrokeCord*) mlt_calloc(1, sizeof(StrokeCord));
    this->count = 0;
    this->chunk_size = chunk_size;
    this->first_chunk = StrokeCord_internal_alloc_chunk(chunk_size);
    if (!this->first_chunk) {
        milton_die_gracefully("StrokeCord construction failed\n");
    }
}
size_t count(StrokeCord& cord)
{
    return cord.count;
}

b32 push(StrokeCord& cord, Stroke elem)
{
    b32 succeeded = false;
    StrokeCord::StrokeCordChunk* chunk = cord.first_chunk;
    size_t chunk_i = cord.count / cord.chunk_size;
    while(chunk_i--) {
        if (!chunk->next) {
            chunk->next = StrokeCord_internal_alloc_chunk(cord.chunk_size);
        }
        chunk = chunk->next;
    }
    if (chunk) {
        size_t elem_i = cord.count % cord.chunk_size;
        chunk->data[elem_i] = elem;
        cord.count += 1;
        succeeded = true;
    }
    return succeeded;
}

Stroke& StrokeCord::operator[](const size_t i)
{
    StrokeCord::StrokeCordChunk* chunk = this->first_chunk;
    size_t chunk_i = i / this->chunk_size;
    while(chunk_i--) {
        chunk = chunk->next;
        assert (chunk != NULL);
    }
    size_t elem_i = i % this->chunk_size;
    return chunk->data[elem_i];
}

Stroke pop(StrokeCord* cord, size_t i)
{
    StrokeCord::StrokeCordChunk* chunk = cord->first_chunk;
    size_t chunk_i = i / cord->chunk_size;
    while(chunk_i--) {
        chunk = chunk->next;
        assert (chunk != NULL);
    }
    size_t elem_i = i % cord->chunk_size;
    Stroke result = chunk->data[elem_i];
    cord->count -= 1;
    return result;
}

