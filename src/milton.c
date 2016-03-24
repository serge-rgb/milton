// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#include "milton.h"

#include "gui.h"
#include "platform.h"
#include "persist.h"
#include "profiler.h"
#include "rasterizer.h"

#if MILTON_DEBUG
#include "tests.h"
#endif

// Using stb_image to load our GUI resources.
#include <stb_image.h>

static void milton_gl_backend_init(MiltonState* milton_state)
{
    GLuint test = 1;
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
            "varying vec2 coord;\n"
            "\n"
            "void main(void)\n"
            "{\n"
            "   vec4 color = texture2D(raster_buffer, coord); \n"
            "   gl_FragColor = color; \n"
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

        GLCHK (glGenBuffers (1, &milton_state->gl->vbo));
        assert (milton_state->gl->vbo > 0);
        GLCHK (glBindBuffer (GL_ARRAY_BUFFER, milton_state->gl->vbo));

        GLint pos_loc     = glGetAttribLocation(milton_state->gl->quad_program, "position");
        GLint sampler_loc = glGetUniformLocation(milton_state->gl->quad_program, "raster_buffer");
        assert (pos_loc     >= 0);
        assert (sampler_loc >= 0);
        GLCHK (glBufferData (GL_ARRAY_BUFFER, sizeof(vert_data), vert_data, GL_STATIC_DRAW));
#if MILTON_USE_VAO
        GLCHK (glVertexAttribPointer (/*attrib location*/(GLuint)pos_loc,
                                      /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                      /*stride*/0, /*ptr*/0));
        GLCHK (glEnableVertexAttribArray ((GLuint)pos_loc) );
#endif
    }
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

static void milton_update_brushes(MiltonState* milton_state)
{
    for (int i = 0; i < BrushEnum_COUNT; ++i ) {
        Brush* brush = &milton_state->brushes[i];
        i32 size = milton_state->brush_sizes[i];
        brush->radius = size * milton_state->view->scale;
        assert(brush->radius < FLT_MAX);
        if (i == BrushEnum_PEN) {
            // Alpha is set by the UI
#if FAST_GAMMA
            brush->color = to_premultiplied(square_to_linear(gui_get_picker_rgb(milton_state->gui)), brush->alpha);
#else
            brush->color = to_premultiplied(sRGB_to_linear(gui_get_picker_rgb(milton_state->gui)), brush->alpha);
#endif
        } else if (i == BrushEnum_ERASER) {
            brush->color = (v4f){ -1, -1, -1, -1, };
            brush->alpha = 1;
        }
    }

    milton_state->working_stroke.brush = milton_state->brushes[BrushEnum_PEN];
}

static Brush milton_get_brush(MiltonState* milton_state)
{
    Brush brush = { 0 };
    if ( milton_state->current_mode == MiltonMode_PEN ) {
        brush = milton_state->brushes[BrushEnum_PEN];
    } else if ( milton_state->current_mode == MiltonMode_ERASER ) {
        brush = milton_state->brushes[BrushEnum_ERASER];
    } else {
        assert (!"Picking brush in invalid mode");
    }
    return brush;
}

static i32* pointer_to_brush_size(MiltonState* milton_state)
{
    i32* ptr = NULL;

    if ( milton_state->current_mode == MiltonMode_PEN ) {
        ptr = &milton_state->brush_sizes[BrushEnum_PEN];
    } else if ( milton_state->current_mode == MiltonMode_ERASER ) {
        ptr = &milton_state->brush_sizes[BrushEnum_ERASER];
    }
    return ptr;
}

static b32 is_user_drawing(MiltonState* milton_state)
{
    b32 result = milton_state->working_stroke.num_points > 0;
    return result;
}

static b32 current_mode_is_for_painting(MiltonState* milton_state)
{
    b32 result = milton_state->current_mode == MiltonMode_PEN || milton_state->current_mode == MiltonMode_ERASER;
    return result;
}

static void milton_stroke_input(MiltonState* milton_state, MiltonInput* input)
{
    if ( input->input_count == 0 ) {
        return;
    }

    //milton_log("Stroke input with %d packets\n", input->input_count);
    milton_state->working_stroke.brush    = milton_get_brush(milton_state);
    milton_state->working_stroke.layer_id = milton_state->view->working_layer_id;

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

        if ( passed_inspection && not_the_first ) {
            i32 in_radius = (i32)(pressure * milton_state->working_stroke.brush.radius);

            // Limit the number of points we check so that we don't mess with the stroke too much.
            int point_window = 4;
            int count = 0;
            // Pop every point that is contained by the new one.
            for ( i32 i = milton_state->working_stroke.num_points - 1; i >= 0; --i ) {
                if ( ++count >= point_window ) {
                    break;
                }
                v2i this_point = milton_state->working_stroke.points[i];
                i32 this_radius = (i32)(milton_state->working_stroke.brush.radius * milton_state->working_stroke.pressures[i]);

                if ( stroke_point_contains_point(canvas_point, in_radius, this_point, this_radius) ) {
                    milton_state->working_stroke.num_points -= 1;
                } else if ( stroke_point_contains_point(this_point, this_radius, canvas_point, in_radius) ) {
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

i32 milton_get_brush_size(MiltonState* milton_state)
{
    i32 brush_size = 0;
    if ( milton_state->current_mode == MiltonMode_PEN ) {
        brush_size = milton_state->brush_sizes[BrushEnum_PEN];
    } else if ( milton_state->current_mode == MiltonMode_ERASER ) {
        brush_size = milton_state->brush_sizes[BrushEnum_ERASER];
    } else {
        assert (! "milton_get_brush_size called when in invalid mode.");
    }
    return brush_size;
}

void milton_set_brush_size(MiltonState* milton_state, i32 size)
{
    if ( current_mode_is_for_painting(milton_state) ) {
        if ( size < k_max_brush_size && size > 0 ) {
            (*pointer_to_brush_size(milton_state)) = size;
            milton_update_brushes(milton_state);
        }
    }
}

// For keyboard shortcut.
void milton_increase_brush_size(MiltonState* milton_state)
{
    if ( current_mode_is_for_painting(milton_state) ) {
        i32 brush_size = milton_get_brush_size(milton_state);
        if (brush_size < k_max_brush_size && brush_size > 0) {
            milton_set_brush_size(milton_state, brush_size + 1);
        }
        milton_update_brushes(milton_state);
    }
}

// For keyboard shortcut.
void milton_decrease_brush_size(MiltonState* milton_state)
{
    if ( current_mode_is_for_painting(milton_state) ) {
        i32 brush_size = milton_get_brush_size(milton_state);

        if (brush_size > 1) {
            milton_set_brush_size(milton_state, brush_size - 1);
        }
        milton_update_brushes(milton_state);
    }
}

void milton_set_pen_alpha(MiltonState* milton_state, float alpha)
{
    milton_state->brushes[BrushEnum_PEN].alpha = alpha;
    milton_update_brushes(milton_state);
}

float milton_get_pen_alpha(MiltonState* milton_state)
{
    const float alpha = milton_state->brushes[BrushEnum_PEN].alpha;
    return alpha;
}

void milton_init(MiltonState* milton_state)
{
    // Initialize render queue
    milton_state->render_stack = arena_alloc_elem(milton_state->root_arena, RenderStack);
    {
        milton_state->render_stack->work_available      = SDL_CreateSemaphore(0);
        milton_state->render_stack->completed_semaphore = SDL_CreateSemaphore(0);
        milton_state->render_stack->mutex               = SDL_CreateMutex();
    }

    milton_state->num_render_workers = SDL_GetCPUCount();
#if RESTRICT_NUM_THREADS_TO_2
    milton_state->num_render_workers = 2;
#endif

    milton_log("[DEBUG]: Creating %d render workers.\n", milton_state->num_render_workers);

    milton_state->render_worker_arenas = arena_alloc_array(milton_state->root_arena,
                                                           milton_state->num_render_workers,
                                                           Arena);
    milton_state->worker_memory_size = 65536;


    assert (milton_state->num_render_workers);

    milton_state->bytes_per_pixel = 4;

    milton_state->working_stroke.points    = arena_alloc_array(milton_state->root_arena, STROKE_MAX_POINTS, v2i);
    milton_state->working_stroke.pressures = arena_alloc_array(milton_state->root_arena, STROKE_MAX_POINTS, f32);

    milton_state->current_mode = MiltonMode_PEN;
    milton_state->last_mode = MiltonMode_NONE;

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
        *milton_state->view = (CanvasView) {
            .scale               = MILTON_DEFAULT_SCALE,
            .downsampling_factor = 1,
            .num_layers          = 1,
            .background_color    = (v3f){ 1, 1, 1 },
        };
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

    // TODO: free-painting function
    milton_new_layer(milton_state);  // Fill out first layer


    milton_gl_backend_init(milton_state);
    milton_load(milton_state);
    milton_state->view->canvas_radius_limit = 1u << 30;

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

#if MILTON_DEBUG
    milton_run_tests(milton_state);
#endif

    for (i32 i = 0; i < milton_state->num_render_workers; ++i) {
        WorkerParams* params = arena_alloc_elem(milton_state->root_arena, WorkerParams);
        {
            *params = (WorkerParams){ milton_state, i };
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

    milton_state->running = true;
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

    if ( do_realloc ) {
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
         new_screen_size.h < milton_state->max_height ) {
        milton_state->view->screen_size = new_screen_size;
        milton_state->view->screen_center = divide2i(milton_state->view->screen_size, 2);

        // Add delta to pan vector
        v2i pan_vector = add2i(milton_state->view->pan_vector, (scale2i(pan_delta, milton_state->view->scale)));

        if ( pan_vector.x > milton_state->view->canvas_radius_limit || pan_vector.x <= -milton_state->view->canvas_radius_limit ) {
            pan_vector.x = milton_state->view->pan_vector.x;
        }
        if ( pan_vector.y > milton_state->view->canvas_radius_limit || pan_vector.y <= -milton_state->view->canvas_radius_limit ) {
            pan_vector.y = milton_state->view->pan_vector.y;
        }
        milton_state->view->pan_vector = pan_vector;
    } else {
        assert(!"DEBUG: new screen size is more than we can handle.");
    }
}

void milton_switch_mode(MiltonState* milton_state, MiltonMode mode)
{
    if ( mode != milton_state->current_mode ) {
        milton_state->last_mode = milton_state->current_mode;
        milton_state->current_mode = mode;

        if ( mode == MiltonMode_EXPORTING && milton_state->gui->visible) {
            gui_toggle_visibility(milton_state->gui);
        }
        if ( milton_state->last_mode == MiltonMode_EXPORTING && !milton_state->gui->visible) {
            gui_toggle_visibility(milton_state->gui);
        }
    }
}

void milton_use_previous_mode(MiltonState* milton_state)
{
    if ( milton_state->last_mode != MiltonMode_NONE ) {
        milton_switch_mode(milton_state, milton_state->last_mode);
    } else {
        assert ( !"invalid code path" );
    }
}
void milton_try_quit(MiltonState* milton_state)
{
    milton_state->running = false;
}

void milton_expand_render_memory(MiltonState* milton_state)
{
    if ( milton_state->worker_needs_memory ) {
        size_t prev_memory_value = milton_state->worker_memory_size;
        milton_state->worker_memory_size *= 2;
        size_t needed_size = milton_state->worker_memory_size;

        for (int i = 0; i < milton_state->num_render_workers; ++i) {
            if (milton_state->render_worker_arenas[i].ptr != NULL) {
                mlt_free(milton_state->render_worker_arenas[i].ptr);
            }
            u8* new_memory = (u8*)mlt_calloc(1, needed_size);
            milton_state->render_worker_arenas[i] = arena_init(new_memory, needed_size);
            if (milton_state->render_worker_arenas[i].ptr == NULL) {
                milton_die_gracefully("Failed to realloc worker arena\n");
            }
        }

        milton_log("[DEBUG] Assigning more memory per worker. From %lu to %lu\n",
                   prev_memory_value,
                   milton_state->worker_memory_size);

        milton_state->worker_needs_memory = false;
    }
}

void milton_new_layer(MiltonState* milton_state)
{
    i32 id = 0; {  // Find highest id;
        Layer* it = milton_state->root_layer;
        while ( it ) {
            if ( it->id >= id ) {
                id = it->id + 1;
            }
            it = it->next;
        }
    }

    Layer* layer = mlt_calloc(1, sizeof(Layer));
    *layer = (Layer) {
        .id = id,
        .name = mlt_calloc(MAX_LAYER_NAME_LEN, sizeof(char)),
        .flags = LayerFlags_VISIBLE,
    };
    snprintf(layer->name, 1024, "Layer %d", layer->id);

    if ( milton_state->root_layer != NULL ) {
        Layer* top = layer_get_topmost(milton_state->root_layer);
        top->next = layer;
        layer->prev = top;
        milton_set_working_layer(milton_state, top->next);
    } else {
        milton_state->root_layer = layer;
        milton_set_working_layer(milton_state, layer);
    }
}

void milton_set_working_layer(MiltonState* milton_state, Layer* layer)
{
    milton_state->working_layer = layer;
    milton_state->view->working_layer_id = layer->id;
}

void milton_delete_working_layer(MiltonState* milton_state)
{
    Layer* layer = milton_state->working_layer;
    if ( layer->next || layer->prev ) {
        if ( layer->next )
            layer->next->prev = layer->prev;
        if ( layer->prev )
            layer->prev->next = layer->next;

        sb_push(milton_state->layer_graveyard, layer);
        sb_push(milton_state->redo_stack, HistoryElement_LAYER);


        if (layer->next)
            milton_state->working_layer = layer->next;
        else
            milton_state->working_layer = layer->prev;
    }
}


void milton_update(MiltonState* milton_state, MiltonInput* input)
{
    // TODO: Save redo point?
    b32 should_save =
            (check_flag(input->flags, MiltonInputFlags_END_STROKE)) ||
            (check_flag(input->flags, MiltonInputFlags_UNDO)) ||
            (check_flag(input->flags, MiltonInputFlags_REDO));

    MiltonRenderFlags render_flags = MiltonRenderFlags_NONE;

    if ( !milton_state->running ) {
        // Someone tried to kill milton from outside the update. Make sure we save.
        should_save = true;
        goto cleanup;
    }

    if ( milton_state->worker_needs_memory ) {
        milton_expand_render_memory(milton_state);
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
    }

    if ( milton_state->request_quality_redraw ) {
        milton_state->view->downsampling_factor = 1;  // See how long it takes to redraw at full quality
        milton_state->request_quality_redraw = false;
        set_flag(render_flags, MiltonRenderFlags_FULL_REDRAW);
    }

    i32 now = SDL_GetTicks();

    if ( check_flag(input->flags, MiltonInputFlags_FAST_DRAW) ) {
        set_flag(render_flags, MiltonRenderFlags_DRAW_ITERATIVELY);
        milton_state->quality_redraw_time = now;
    }
    else if ( milton_state->quality_redraw_time > 0 &&
              (now - milton_state->quality_redraw_time) > QUALITY_REDRAW_TIMEOUT_MS ) {
        milton_state->request_quality_redraw = true;  // Next update loop.
        milton_state->quality_redraw_time = 0;
    }

    if (check_flag(input->flags, MiltonInputFlags_FULL_REFRESH)) {
        set_flag(render_flags, MiltonRenderFlags_FULL_REDRAW);
    }

    if (input->scale) {
        set_flag(render_flags, MiltonRenderFlags_FULL_REDRAW);

// Sensible
#if 1
        f32 scale_factor = 1.3f;
        i32 view_scale_limit = (1 << 15);
// Debug
#else
        f32 scale_factor = 1.5f;
        i32 view_scale_limit = 1 << 20;
#endif

        // Some info on the reasoning behind choosing the values for different
        // knobs regarding scales.  We use the whole 32 bit address space for
        // integer coordinates, but there are tricky trade-offs between zoom
        // level and canvas length.  When fully zoomed-out, the canvas might be
        // too small in area available to pan. When fully zoomed-in, we run
        // into problems with the "arbitrary size" exporting. The latter
        // problem is trickier, but it is easy to see when imagining that at
        // full zoom-in, each screen pixel corresponds to a canvas unit. When
        // exporting at this scale, we can't stretch the canvas anymore! A
        // possible solution to this is to scale-up the strokes relative to the
        // center of the exporting rectangle. Another "solution" is to set a
        // maximum exporting scale coefficient, and set the minimum zoom to be
        // that as well.  The former solution is the more flexible. Taken to
        // its fullest potential it would allow a greater level of zoom-in. It
        // introduces more complexity, though.
        //
        // The current strategy is to enforece a min_scale
#if 0
        {
            int w = 2560;
            f32 ns = (f32)(1u << 31) / (w * view_scale_limit);
            f32 ms = (f32)(1u << 31) / (w * milton_state->view->scale);

            milton_log("With a screen width of %d, At this zoomout we would have %f horizontal screens. \n"
                       "At max zoomout we would have %f horizontal screens of movement.\n", w, ms, ns);

            i64 maxr = (1u << 31) - k_max_brush_size * view_scale_limit;
            milton_log ("max canvas radius is %ld\n", maxr);
        }
#endif

        i32 min_scale = MILTON_MINIMUM_SCALE;

        if ( input->scale > 0 && milton_state->view->scale >= min_scale ) {
            milton_state->view->scale = (i32)(ceilf(milton_state->view->scale / scale_factor));
        } else if ( input->scale < 0 && milton_state->view->scale < view_scale_limit ) {
            milton_state->view->scale = (i32)(milton_state->view->scale * scale_factor) + 1;
        }
        milton_update_brushes(milton_state);
    } else if ( check_flag(input->flags, MiltonInputFlags_PANNING )) {
        // If we are *not* zooming and we are panning, we can copy most of the
        // framebuffer
        if ( !equ2i(input->pan_delta, (v2i){0}) ) {
            set_flag(render_flags, MiltonRenderFlags_PAN_COPY);
        }
    }

    if ( check_flag(input->flags, MiltonInputFlags_CHANGE_MODE) ) {
        milton_switch_mode( milton_state, input->mode_to_set );
        if ( input->mode_to_set == MiltonMode_PEN ||
             input->mode_to_set == MiltonMode_ERASER ) {
            milton_update_brushes(milton_state);
        }
    }

#if 0
    {
        if (check_flag( input->flags, MiltonInputFlags_UNDO )) {
            Stroke stroke = pop_latest_stroke(milton_state);
            sb_push(milton_state->redo_stack, stroke);
            set_flag(render_flags, MiltonRenderFlags_FULL_REDRAW);
        }
        else if ( check_flag(input->flags, MiltonInputFlags_REDO ) ) {
            if (sb_count(milton_state->redo_stack) > 0) {
                layer_push_stroke(milton_state, sb_pop(milton_state->redo_stack));
                set_flag(render_flags, MiltonRenderFlags_FULL_REDRAW);
            }
        }
    }
#endif

#if 0
    // ==== Rotate ======
    if (input->rotation != 0)
    {
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
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

    // If the current mode is Pen or Eraser, we show the hover. It can be unset under various conditions later.
    if ( milton_state->current_mode == MiltonMode_PEN ||
         milton_state->current_mode == MiltonMode_ERASER ) {
        set_flag(render_flags, MiltonRenderFlags_BRUSH_HOVER);
    }

    if (check_flag( input->flags, MiltonInputFlags_HOVERING )) {
        milton_state->hover_point = input->hover_point;
        f32 x = input->hover_point.x / (f32)milton_state->view->screen_size.w;
        f32 y = input->hover_point.y / (f32)milton_state->view->screen_size.w;
    }

    if ( is_user_drawing(milton_state) || milton_state->gui->active ) {
        unset_flag(render_flags, MiltonRenderFlags_BRUSH_HOVER);
    }

    if ( input->input_count > 0 ){
        if ( current_mode_is_for_painting(milton_state) ) {
            if ( !is_user_drawing(milton_state) && gui_consume_input(milton_state->gui, input) ) {
                milton_update_brushes(milton_state);
                set_flag(render_flags, gui_process_input(milton_state, input));
            } else if (!milton_state->gui->active) {
                milton_stroke_input(milton_state, input);
            }

            // Clear redo stack
            sb_reset(milton_state->redo_stack);
        }
    }

    if ( milton_state->current_mode == MiltonMode_EXPORTING ) {
        exporter_input(&milton_state->gui->exporter, input);
    }

    if ( check_flag(input->flags, MiltonInputFlags_IMGUI_GRABBED_INPUT) ) {
        // Start drawing the preview if we just grabbed a slider.
        unset_flag(render_flags, MiltonRenderFlags_BRUSH_HOVER);
        if ( check_flag(milton_state->gui->flags, MiltonGuiFlags_SHOWING_PREVIEW) ) {
            set_flag(render_flags, MiltonRenderFlags_BRUSH_PREVIEW);
        }
    } else {
        gui_imgui_set_ungrabbed(milton_state->gui);
    }

    if (check_flag( input->flags, MiltonInputFlags_END_STROKE )) {
        milton_state->stroke_is_from_tablet = false;

        if ( milton_state->gui->active ) {
            gui_deactivate(milton_state->gui);
            set_flag(render_flags, MiltonRenderFlags_PICKER_UPDATED);
            unset_flag(render_flags, MiltonRenderFlags_BRUSH_HOVER);
        } else {
            if ( milton_state->working_stroke.num_points > 0 ) {
                // We used the selected color to draw something. Push.
                if( gui_mark_color_used(milton_state->gui, milton_state->working_stroke.brush.color.rgb) ) {
                    set_flag(render_flags, MiltonRenderFlags_PICKER_UPDATED);
                }
                // Copy current stroke.
                i32 num_points = milton_state->working_stroke.num_points;
                Stroke new_stroke =
                {
                    .brush = milton_state->working_stroke.brush,
                    .points = (v2i*)mlt_calloc((size_t)num_points, sizeof(v2i)),
                    .pressures = (f32*)mlt_calloc((size_t)num_points, sizeof(f32)),
                    .num_points = num_points,
                    .layer_id = milton_state->view->working_layer_id,
                };

                memcpy(new_stroke.points, milton_state->working_stroke.points, milton_state->working_stroke.num_points * sizeof(v2i));
                memcpy(new_stroke.pressures, milton_state->working_stroke.pressures, milton_state->working_stroke.num_points * sizeof(f32));

                layer_push_stroke(milton_state->working_layer, new_stroke);
                // Clear working_stroke
                {
                    milton_state->working_stroke.num_points = 0;
                }

                set_flag(render_flags, MiltonRenderFlags_FINISHED_STROKE);
            }
        }
    }

    // Disable hover if panning.
    if (check_flag( input->flags, MiltonInputFlags_PANNING )) {
        unset_flag(render_flags, MiltonRenderFlags_BRUSH_HOVER);
    }

    milton_render(milton_state, render_flags, input->pan_delta);

cleanup:
    if ( should_save ) {
        milton_save(milton_state);
    }
    profiler_output();
}
