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



#include "milton.h"

#include "profiler.h"

#ifndef NDEBUG
#include "tests.h"
#endif

static void milton_gl_backend_init(MiltonState* milton_state)
{
    // Init quad program
    {
        const char* shader_contents[2];
        shader_contents[0] =
            "#version 120\n"
            "attribute vec2 position;\n"
            "\n"
            "varying vec2 coord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "   coord = (position + vec2(1.0,1.0))/2.0;\n"
            "   coord.y = 1.0 - coord.y;"
            "   // direct to clip space. must be in [-1, 1]^2\n"
            "   gl_Position = vec4(position, 0.0, 1.0);\n"
            "}\n";

        shader_contents[1] =
            "#version 120\n"
            "\n"
            "uniform sampler2D raster_buffer;\n"
            "uniform float aspect_ratio;\n"
            "varying vec2 coord;\n"
            "\n"
            "void main(void)\n"
            "{\n"
            "   vec4 color = texture2D(raster_buffer, coord).bgra; \n"
            "   gl_FragColor = color; \n"
            //"   out_color = color; \n"
            "}\n";

        GLuint shader_objects[2] = {0};
        for ( int i = 0; i < 2; ++i ) {
            GLuint shader_type = (GLuint)((i == 0) ? GL_VERTEX_SHADER_ARB : GL_FRAGMENT_SHADER_ARB);
            shader_objects[i] = gl_compile_shader(shader_contents[i], shader_type);
        }
        milton_state->gl->quad_program = glCreateProgramObjectARB();
        gl_link_program(milton_state->gl->quad_program, shader_objects, 2);

        GLCHK (glUseProgramObjectARB(milton_state->gl->quad_program));
    }

    // Create texture
    {
         GLCHK (glActiveTexture (GL_TEXTURE0) );
        // Create texture
        GLCHK (glGenTextures   (1, &milton_state->gl->texture));
        GLCHK (glBindTexture   (GL_TEXTURE_2D, milton_state->gl->texture));

        // Note for the future: These are needed.
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    }
    // Create quad
    {
#define u -1.0f
        // full
        GLfloat vert_data[] = {
            -u, +u,
            -u, -u,
            +u, -u,
            +u, +u,
        };
#undef u
#if MILTON_USE_VAO
        GLCHK (glGenVertexArrays(1, &milton_state->gl->quad_vao));
        GLCHK (glBindVertexArray(milton_state->gl->quad_vao));
#endif

        GLCHK (glGenBuffersARB(1, &milton_state->gl->vbo));
        assert (milton_state->gl->vbo > 0);
        GLCHK (glBindBufferARB(GL_ARRAY_BUFFER, milton_state->gl->vbo));

        GLint pos_loc     = glGetAttribLocationARB(milton_state->gl->quad_program, "position");
        GLint sampler_loc = glGetUniformLocationARB(milton_state->gl->quad_program, "raster_buffer");
        assert (pos_loc     >= 0);
        assert (sampler_loc >= 0);
        GLCHK (glBufferDataARB (GL_ARRAY_BUFFER, sizeof(vert_data), vert_data, GL_STATIC_DRAW));
#if MILTON_USE_VAO
        GLCHK (glVertexAttribPointerARB(/*attrib location*/pos_loc,
                                        /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                        /*stride*/0, /*ptr*/0));
        GLCHK (glEnableVertexAttribArrayARB (pos_loc) );
#endif
    }
}

i32 milton_get_brush_size(const MiltonState& milton_state)
{
    i32 brush_size = 0;
    if (check_flag(milton_state.current_mode, MiltonMode::PEN)) {
        brush_size = milton_state.brush_sizes[BrushEnum_PEN];
    } else if (check_flag(milton_state.current_mode, MiltonMode::ERASER)) {
        brush_size = milton_state.brush_sizes[BrushEnum_ERASER];
    } else {
        assert (! "milton_get_brush_size called when in invalid mode.");
    }
    return brush_size;
}

static void milton_update_brushes(MiltonState* milton_state)
{
    for (int i = 0; i < BrushEnum_COUNT; ++i ) {
        Brush* brush = &milton_state->brushes[i];
        i32 size = milton_state->brush_sizes[i];
        brush->radius = size * milton_state->view->scale;
        assert(brush->radius < FLT_MAX);
        if (i == BrushEnum_PEN) {
            // Alpha is set by the UI
            brush->color = to_premultiplied(sRGB_to_linear(gui_get_picker_rgb(milton_state->gui)),
                                            brush->alpha);
        } else if (i == BrushEnum_ERASER) {
            brush->color = { 1, 1, 1, 1, };
            brush->alpha = 1;
        }
    }

    milton_state->working_stroke.brush = milton_state->brushes[BrushEnum_PEN];
}

static Brush milton_get_brush(MiltonState* milton_state)
{
    Brush brush = { 0 };
    if (check_flag(milton_state->current_mode, MiltonMode::PEN)) {
        brush = milton_state->brushes[BrushEnum_PEN];
    } else if (check_flag(milton_state->current_mode, MiltonMode::ERASER)) {
        brush = milton_state->brushes[BrushEnum_ERASER];
    } else {
        assert (!"Picking brush in invalid mode");
    }
    return brush;
}

static i32* pointer_to_brush_size(MiltonState* milton_state)
{
    i32* ptr = NULL;

    if (check_flag(milton_state->current_mode, MiltonMode::PEN)) {
        ptr = &milton_state->brush_sizes[BrushEnum_PEN];
    } else if (check_flag(milton_state->current_mode, MiltonMode::ERASER)) {
        ptr = &milton_state->brush_sizes[BrushEnum_ERASER];
    }
    return ptr;
}

void milton_gl_backend_draw(MiltonState* milton_state)
{
    MiltonGLState* gl = milton_state->gl;
    u8* raster_buffer = milton_state->raster_buffer;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)raster_buffer);

    GLCHK (glUseProgramObjectARB(gl->quad_program));
#if MILTON_USE_VAO
    glBindVertexArray(gl->quad_vao);
#else
    GLint pos_loc     = glGetAttribLocationARB(gl->quad_program, "position");
    GLint sampler_loc = glGetUniformLocationARB(gl->quad_program, "raster_buffer");
    assert (pos_loc     >= 0);
    assert (sampler_loc >= 0);
    GLCHK (glUniform1iARB(sampler_loc, 0 /*GL_TEXTURE0*/));
    GLCHK (glVertexAttribPointerARB(/*attrib location*/(GLuint)pos_loc,
                                    /*size*/2, GL_FLOAT,
                                    /*normalize*/ GL_FALSE,
                                    /*stride*/0,
                                    /*ptr*/0));

    GLCHK (glEnableVertexAttribArrayARB((GLuint)pos_loc));
#endif
    GLCHK (glDrawArrays (GL_TRIANGLE_FAN, 0, 4) );
}


void milton_set_brush_size(MiltonState& milton_state, i32 size)
{
    if (size < k_max_brush_size && size > 0) {
        (*pointer_to_brush_size(&milton_state)) = size;
        milton_update_brushes(&milton_state);
    }
}

// For keyboard shortcut.
void milton_increase_brush_size(MiltonState* milton_state)
{
    i32 brush_size = milton_get_brush_size(*milton_state);
    if (brush_size < k_max_brush_size && brush_size > 0) {
        milton_set_brush_size(*milton_state, brush_size + 1);
    }
    milton_update_brushes(milton_state);
}

// For keyboard shortcut.
void milton_decrease_brush_size(MiltonState* milton_state)
{
    i32 brush_size = milton_get_brush_size(*milton_state);

    if (brush_size > 1) {
        milton_set_brush_size(*milton_state, brush_size - 1);
    }
    milton_update_brushes(milton_state);
}

void milton_set_pen_alpha(MiltonState* milton_state, float alpha)
{
    milton_state->brushes[BrushEnum_PEN].alpha = alpha;
    milton_update_brushes(milton_state);
}

float milton_get_pen_alpha(const MiltonState& milton_state)
{
    const float alpha = milton_state.brushes[BrushEnum_PEN].alpha;
    return alpha;
}

static void milton_load_assets(MiltonState* milton_state)
{
    MiltonGui* gui = milton_state->gui;

    Bitmap* bitmap = &gui->brush_button.bitmap;

    static char* img_name_brush_button = "assets/brush.png";

    bitmap->data = stbi_load(img_name_brush_button, &bitmap->width, &bitmap->height,
                             &bitmap->num_components, 4);
    i32 x = 400;
    i32 y = 500;
    gui->brush_button.rect = rect_from_xywh(x, y, bitmap->width, bitmap->height);
}

void milton_init(MiltonState* milton_state)
{
    // Fill cpu capabilities.
    milton_state->cpu_caps = CPUCaps::none;
    if ( SDL_HasSSE2() ) {
        set_flag(milton_state->cpu_caps, CPUCaps::sse2);
    }
    if ( SDL_HasAVX() ) {
        set_flag(milton_state->cpu_caps, CPUCaps::avx);
    }

    // Initialize render queue
    milton_state->render_queue = arena_alloc_elem(milton_state->root_arena, RenderQueue);
    {
        milton_state->render_queue->work_available      = SDL_CreateSemaphore(0);
        milton_state->render_queue->completed_semaphore = SDL_CreateSemaphore(0);
        milton_state->render_queue->mutex               = SDL_CreateMutex();
    }

    milton_state->num_render_workers = SDL_GetCPUCount();

    milton_log("[DEBUG]: Creating %d render workers.\n", milton_state->num_render_workers);

    milton_state->render_worker_arenas = arena_alloc_array(milton_state->root_arena,
                                                           milton_state->num_render_workers,
                                                           Arena);
    milton_state->worker_memory_size = 65536;

    assert (milton_state->num_render_workers);

    // Allocate enough memory for the maximum possible supported resolution. As
    // of now, it seems like future 8k displays will adopt this resolution.
    milton_state->bytes_per_pixel = 4;

    milton_state->strokes = StrokeCord(1024);

    milton_state->working_stroke.points    = arena_alloc_array(milton_state->root_arena, STROKE_MAX_POINTS, v2i);
    milton_state->working_stroke.pressures = arena_alloc_array(milton_state->root_arena, STROKE_MAX_POINTS, f32);

    milton_state->current_mode = MiltonMode::PEN;

    milton_state->gl = arena_alloc_elem(milton_state->root_arena, MiltonGLState);

#if 1
    milton_state->blocks_per_blockgroup = 16;
    milton_state->block_width = 32;
#else
    milton_state->blocks_per_blockgroup = 4;
    milton_state->block_width = 32;
#endif

    // Set the view
    {
        milton_state->view = arena_alloc_elem(milton_state->root_arena, CanvasView);
        milton_state->view->scale = MILTON_DEFAULT_SCALE;
        milton_state->view->downsampling_factor = 1;
        milton_state->view->canvas_radius_limit = 1024 * 1024 * 512;
#if 0
        milton_state->view->rotation = 0;
        for (int d = 0; d < 360; d++)
        {
            f32 r = deegrees_to_radians(d);
            f32 c = cosf(r);
            f32 s = sinf(r);
            milton_state->view->cos_sin_table[d][0] = c;
            milton_state->view->cos_sin_table[d][1] = s;
        }
#endif
    }

    milton_state->gui = arena_alloc_elem(milton_state->root_arena, MiltonGui);
    gui_init(milton_state->root_arena, milton_state->gui);

    milton_gl_backend_init(milton_state);
    milton_load(milton_state);

    // Set default brush sizes.
    for (int i = 0; i < BrushEnum_COUNT; ++i) {
        switch (i) {
        case BrushEnum_PEN:
            milton_state->brush_sizes[i] = 10;
            break;
        case BrushEnum_ERASER:
            milton_state->brush_sizes[i] = 40;
            break;
        default:
            assert(!"New brush has not been given a default size");
            break;
        }
    }

    milton_set_pen_alpha(milton_state, 0.8f);
    milton_run_tests(milton_state);

    for (i32 i = 0; i < milton_state->num_render_workers; ++i) {
        WorkerParams* params = arena_alloc_elem(milton_state->root_arena, WorkerParams);
        {
            *params = { milton_state, i };
        }
        assert (milton_state->render_worker_arenas[i].ptr == NULL);
        u8* worker_memory = (u8*)mlt_calloc(1, milton_state->worker_memory_size);
        if (!worker_memory) {
            milton_die_gracefully("Platform allocation failed");
        }
        milton_state->render_worker_arenas[i] = arena_init(worker_memory,
                                                           milton_state->worker_memory_size);

        SDL_CreateThread(renderer_worker_thread, "Milton Render Worker", (void*)params);
    }
    milton_load_assets(milton_state);
}

static b32 is_user_drawing(MiltonState* milton_state)
{
    b32 result = milton_state->working_stroke.num_points > 0;
    return result;
}


static void milton_stroke_input(MiltonState* milton_state, MiltonInput* input)
{
    if ( input->input_count == 0 ) {
        return;
    }

    //milton_log("Stroke input with %d packets\n", input->input_count);
    milton_state->working_stroke.brush = milton_get_brush(milton_state);

    //int input_i = 0;
    for (int input_i = 0; input_i < input->input_count; ++input_i) {
        v2i in_point = input->points[input_i];

        v2i canvas_point = raster_to_canvas(milton_state->view, in_point);

        f32 pressure = NO_PRESSURE_INFO;

        if (input->pressures[input_i] != NO_PRESSURE_INFO)
        {
            f32 pressure_min = 0.20f;
            pressure = pressure_min + input->pressures[input_i] * (1.0f - pressure_min);
            milton_state->stroke_is_from_tablet = true;
        }

        // We haven't received pressure info, assume mouse input
        if (input->pressures[input_i] == NO_PRESSURE_INFO && !milton_state->stroke_is_from_tablet)
        {
            pressure = 1.0f;
        }

        b32 not_the_first = false;
        if ( milton_state->working_stroke.num_points >= 1 ) {
            not_the_first = true;
        }

        // A point passes inspection if:
        //  a) it's the first point of this stroke
        //  b) it is being appended to the stroke and it didn't merge with the previous point.
        b32 passed_inspection = true;

        if ( pressure == NO_PRESSURE_INFO ) {
            passed_inspection = false;
        }

        if (passed_inspection && not_the_first) {
            i32 in_radius = (i32)(pressure * milton_state->working_stroke.brush.radius);

            // Limit the number of points we check so that we don't mess with the stroke too much.
            int point_window = 4;
            int count = 0;
            // Pop every point that is contained by the new one.
            for (i32 i = milton_state->working_stroke.num_points - 1; i >= 0; --i) {
                if ( ++count >= point_window ) {
                    break;
                }
                /* v2i this_point = (v2i){milton_state->working_stroke.points_x[i], milton_state->working_stroke.points_y[i]}; */
                /* i32 this_radius = (i32)(milton_state->working_stroke.brush.radius * milton_state->working_stroke.pressures[i]); */
                v2i this_point = milton_state->working_stroke.points[i];
                i32 this_radius = (i32)(milton_state->working_stroke.brush.radius * milton_state->working_stroke.pressures[i]);

                if ( stroke_point_contains_point(canvas_point, in_radius,
                                                 this_point, this_radius) ) {
                    milton_state->working_stroke.num_points -= 1;
                } else if ( stroke_point_contains_point(this_point, this_radius,
                                                        canvas_point, in_radius) ) {
                    // If some other point in the past contains this point,
                    // then this point is invalid.
                    passed_inspection = false;
                    break;
                }
            }
        }

        // Cleared to be appended.
        if ( passed_inspection ) {
            // Add to current stroke.
            int index = milton_state->working_stroke.num_points++;
            milton_state->working_stroke.points[index] = canvas_point;
            milton_state->working_stroke.pressures[index] = pressure;
        }
    }
}

void milton_resize(MiltonState* milton_state, v2i pan_delta, v2i new_screen_size)
{
    if ((new_screen_size.w > 8000 ||
         new_screen_size.h > 8000 ||
         new_screen_size.w <= 0 ||
         new_screen_size.h <= 0) ) {
        return;
    }

    b32 do_realloc = false;
    if (milton_state->max_width <= new_screen_size.w) {
        milton_state->max_width = new_screen_size.w + 256;
        do_realloc = true;
    }
    if (milton_state->max_height <= new_screen_size.h) {
        milton_state->max_height = new_screen_size.h + 256;
        do_realloc = true;
    }

    size_t buffer_size = (size_t) milton_state->max_width * milton_state->max_height * milton_state->bytes_per_pixel;

    if (do_realloc) {
        u8* raster_buffer = milton_state->raster_buffer;
        u8* canvas_buffer = milton_state->canvas_buffer;
        if ( raster_buffer ) mlt_free(raster_buffer);
        if ( canvas_buffer ) mlt_free(canvas_buffer);
        milton_state->raster_buffer      = (u8*)mlt_calloc(1, buffer_size);
        milton_state->canvas_buffer      = (u8*)mlt_calloc(1, buffer_size);

        // TODO: handle this failure gracefully.
        assert(milton_state->raster_buffer);
        assert(milton_state->canvas_buffer);
    }

    if ( new_screen_size.w < milton_state->max_width &&
         new_screen_size.h < milton_state->max_height) {
        milton_state->view->screen_size = new_screen_size;
        milton_state->view->aspect_ratio = (f32)new_screen_size.w / (f32)new_screen_size.h;
        milton_state->view->screen_center = milton_state->view->screen_size/2;

        // Add delta to pan vector
        v2i pan_vector = milton_state->view->pan_vector + (pan_delta*milton_state->view->scale);

        while (pan_vector.x > milton_state->view->canvas_radius_limit)
        {
            pan_vector.x -= milton_state->view->canvas_radius_limit;
        }
        while (pan_vector.x <= -milton_state->view->canvas_radius_limit)
        {
            pan_vector.x += milton_state->view->canvas_radius_limit;
        }
        while (pan_vector.y > milton_state->view->canvas_radius_limit)
        {
            pan_vector.y -= milton_state->view->canvas_radius_limit;
        }
        while (pan_vector.y <= -milton_state->view->canvas_radius_limit)
        {
            pan_vector.y += milton_state->view->canvas_radius_limit;
        }
        milton_state->view->pan_vector = pan_vector;
    } else {
        assert(!"DEBUG: new screen size is more than we can handle.");
    }
}

void milton_update(MiltonState* milton_state, MiltonInput* input)
{
    // TODO: Save redo point?
    b32 should_save =
            //(input->point != NULL) ||
            (check_flag(input->flags, MiltonInputFlags::END_STROKE)) ||
            (check_flag(input->flags, MiltonInputFlags::RESET)) ||
            (check_flag(input->flags, MiltonInputFlags::UNDO)) ||
            (check_flag(input->flags, MiltonInputFlags::REDO));

    MiltonRenderFlags render_flags = MiltonRenderFlags::NONE;

    b32 do_fast_draw = false;
    if ( milton_state->worker_needs_memory ) {
        size_t prev_memory_value = milton_state->worker_memory_size;
        milton_state->worker_memory_size *= 2;
        size_t needed_size = milton_state->worker_memory_size;

        for (int i = 0; i < milton_state->num_render_workers; ++i)
        {
            if (milton_state->render_worker_arenas[i].ptr != NULL) {
                mlt_free(milton_state->render_worker_arenas[i].ptr);
            }
            u8* new_memory = (u8*)mlt_calloc(1, needed_size);
            milton_state->render_worker_arenas[i] = arena_init(new_memory, needed_size);
            if (milton_state->render_worker_arenas[i].ptr == NULL)
            {
                milton_die_gracefully("Failed to realloc worker arena\n");
            }
        }

        milton_log("[DEBUG] Assigning more memory per worker. From %lu to %lu\n",
                   prev_memory_value,
                   milton_state->worker_memory_size);

        milton_state->worker_needs_memory = false;
        set_flag(render_flags, MiltonRenderFlags::FULL_REDRAW);
        do_fast_draw = true;
    }

    if (check_flag(input->flags, MiltonInputFlags::FAST_DRAW)) {
        do_fast_draw = true;
    }

    if ( do_fast_draw ) {
        milton_state->view->downsampling_factor = 2;
        set_flag(milton_state->current_mode, MiltonMode::REQUEST_QUALITY_REDRAW);
    } else {
        milton_state->view->downsampling_factor = 1;
        if (check_flag(milton_state->current_mode, MiltonMode::REQUEST_QUALITY_REDRAW)) {
            unset_flag(milton_state->current_mode, MiltonMode::REQUEST_QUALITY_REDRAW);
            set_flag(render_flags, MiltonRenderFlags::FULL_REDRAW);
        }
    }

    if (check_flag(input->flags, MiltonInputFlags::FULL_REFRESH)) {
        set_flag(render_flags, MiltonRenderFlags::FULL_REDRAW);
    }

    if (input->scale) {
        set_flag(render_flags, MiltonRenderFlags::FULL_REDRAW);

// Sensible
#if 1
        f32 scale_factor = 1.3f;
        i32 view_scale_limit = 10000;
// Debug
#else
        f32 scale_factor = 1.5f;
        i32 view_scale_limit = 1000000;
#endif

        static b32 debug_scale_lock = false;
        if ( !debug_scale_lock && input->scale > 0 && milton_state->view->scale >= 2 ) {
            milton_state->view->scale = (i32)(milton_state->view->scale / scale_factor);
            if (milton_state->view->scale == 1)
            {
                debug_scale_lock = true;
            }
        } else if ( input->scale < 0 && milton_state->view->scale < view_scale_limit ) {
            debug_scale_lock = false;
            milton_state->view->scale = (i32)(milton_state->view->scale * scale_factor) + 1;
        }
        milton_update_brushes(milton_state);
    } else if (check_flag(input->flags, MiltonInputFlags::PANNING)) {
        // If we are *not* zooming and we are panning, we can copy most of the
        // framebuffer
        // TODO: What happens with request_quality_redraw?
        set_flag(render_flags, MiltonRenderFlags::PAN_COPY);
    }

    if (check_flag( input->flags, MiltonInputFlags::SET_MODE_PEN )) {
        set_flag(milton_state->current_mode, MiltonMode::PEN);
        unset_flag(milton_state->current_mode, MiltonMode::ERASER);
        milton_update_brushes(milton_state);
    }
    if (check_flag( input->flags, MiltonInputFlags::SET_MODE_ERASER )) {
        set_flag(milton_state->current_mode, MiltonMode::ERASER);
        unset_flag(milton_state->current_mode, MiltonMode::PEN);
        milton_update_brushes(milton_state);
    }

    if (check_flag( input->flags, MiltonInputFlags::UNDO )) {
        if ( milton_state->working_stroke.num_points == 0 && milton_state->strokes.count > 0 ) {
            milton_state->strokes.count--;
            milton_state->num_redos++;
        } else if ( milton_state->working_stroke.num_points > 0 ) {
            assert(!"NPE");
        }
        set_flag(render_flags, MiltonRenderFlags::FULL_REDRAW);
    } else if (check_flag( input->flags, MiltonInputFlags::REDO )) {
        if ( milton_state->num_redos > 0 ) {
            milton_state->strokes.count++;
            milton_state->num_redos--;
        }
        set_flag(render_flags, MiltonRenderFlags::FULL_REDRAW);
    }

    if (check_flag( input->flags, MiltonInputFlags::RESET )) {
        milton_state->view->scale = MILTON_DEFAULT_SCALE;
        set_flag(render_flags, MiltonRenderFlags::FULL_REDRAW);
        milton_state->strokes.count = 0;
        (&milton_state->strokes[0])->num_points = 0;
        milton_state->working_stroke.num_points = 0;
        milton_update_brushes(milton_state);
    }
#if 0
    // ==== Rotate ======
    if (input->rotation != 0)
    {
        render_flags |= MiltonRenderFlags::FULL_REDRAW;
    }
    milton_state->view->rotation += input->rotation;
    while (milton_state->view->rotation < 0)
    {
        milton_state->view->rotation += 360;
    }
    while (milton_state->view->rotation >= 360)
    {
        milton_state->view->rotation -= 360;
    }
#endif

    // By default, draw hover outling. Various cases can unset this flag.
    set_flag(render_flags, MiltonRenderFlags::BRUSH_HOVER);

    if (check_flag( input->flags, MiltonInputFlags::HOVERING )) {
        milton_state->hover_point = input->hover_point;
        f32 x = input->hover_point.x / (f32)milton_state->view->screen_size.w;
        f32 y = input->hover_point.y / (f32)milton_state->view->screen_size.w;
    }

    if ( input->input_count > 0 ) {
        // Don't draw brush outline.
        unset_flag(render_flags, MiltonRenderFlags::BRUSH_HOVER);

        if ( !is_user_drawing(milton_state) && gui_consume_input(milton_state->gui, input) ) {
            milton_update_brushes(milton_state);
            set_flag(render_flags, gui_process_input(milton_state, input));
        } else if (!milton_state->gui->active) {
            milton_stroke_input(milton_state, input);
        }

        // Clear redo stack
        milton_state->num_redos = 0;
    }

    if ( check_flag(input->flags, MiltonInputFlags::IMGUI_GRABBED_INPUT) ) {
        // Start drawing the preview if we just grabbed a slider.
        unset_flag(render_flags, MiltonRenderFlags::BRUSH_HOVER);
        if ( milton_state->gui->is_showing_preview ) {
            set_flag(render_flags, MiltonRenderFlags::BRUSH_PREVIEW);
        }
    } else {
        gui_imgui_set_ungrabbed(*milton_state->gui);
    }

    if (check_flag( input->flags, MiltonInputFlags::END_STROKE )) {
        milton_state->stroke_is_from_tablet = false;

        if ( milton_state->gui->active ) {
            gui_deactivate(milton_state->gui);
            set_flag(render_flags, MiltonRenderFlags::PICKER_UPDATED);
        } else {
            if ( milton_state->working_stroke.num_points > 0 ) {
                // We used the selected color to draw something. Push.
                if( gui_mark_color_used(milton_state->gui, milton_state->working_stroke.brush.color.rgb) ) {
                    set_flag(render_flags, MiltonRenderFlags::PICKER_UPDATED);
                }
                // Copy current stroke.
                auto num_points = milton_state->working_stroke.num_points;
                Stroke new_stroke =
                {
                    milton_state->working_stroke.brush,
                    (v2i*)mlt_calloc((size_t)num_points, sizeof(v2i)),
                    (f32*)mlt_calloc((size_t)num_points, sizeof(f32)),
                    num_points,
                };
                memcpy(new_stroke.points, milton_state->working_stroke.points, milton_state->working_stroke.num_points * sizeof(v2i));
                memcpy(new_stroke.pressures, milton_state->working_stroke.pressures, milton_state->working_stroke.num_points * sizeof(f32));

                push(milton_state->strokes, new_stroke);
                // Clear working_stroke
                {
                    milton_state->working_stroke.num_points = 0;
                }
                set_flag(render_flags, MiltonRenderFlags::FINISHED_STROKE);
            }
        }
    }

    // Disable hover if panning.
    if (check_flag( input->flags, MiltonInputFlags::PANNING )) {
        unset_flag(render_flags, MiltonRenderFlags::BRUSH_HOVER);
    }

    milton_render(milton_state, render_flags);

    if ( should_save ) {
        milton_save(milton_state);
    }
    profiler_output();
}
