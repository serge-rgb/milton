// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


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
            "varying vec2 coord;\n"
            "\n"
            "void main(void)\n"
            "{\n"
            "   vec4 color = texture2D(raster_buffer, coord); \n"
            "   gl_FragColor = color; \n"
            "}\n";

        GLuint shader_objects[2] = {0};
        for ( int i = 0; i < 2; ++i )
        {
            GLuint shader_type = (GLuint)((i == 0) ? GL_VERTEX_SHADER_ARB : GL_FRAGMENT_SHADER_ARB);
            shader_objects[i] = gl_compile_shader(shader_contents[i], shader_type);
        }
        milton_state->gl->quad_program = glCreateProgram();
        gl_link_program(milton_state->gl->quad_program, shader_objects, 2);

        GLCHK (glUseProgram(milton_state->gl->quad_program));
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
        GLfloat vert_data[] =
        {
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
        mlt_assert (milton_state->gl->vbo > 0);
        GLCHK (glBindBuffer (GL_ARRAY_BUFFER, milton_state->gl->vbo));

        GLint pos_loc     = glGetAttribLocation(milton_state->gl->quad_program, "position");
        GLint sampler_loc = glGetUniformLocation(milton_state->gl->quad_program, "raster_buffer");
        mlt_assert (pos_loc     >= 0);
        mlt_assert (sampler_loc >= 0);
        GLCHK (glBufferData (GL_ARRAY_BUFFER, sizeof(vert_data), vert_data, GL_STATIC_DRAW));
#if MILTON_USE_VAO
        GLCHK (glVertexAttribPointer (/*attrib location*/(GLuint)pos_loc,
                                      /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                      /*stride*/0, /*ptr*/0));
        GLCHK (glEnableVertexAttribArray ((GLuint)pos_loc) );
#endif
    }
}

void milton_set_background_color(MiltonState* milton_state, v3f background_color)
{
    milton_state->view->background_color = background_color;
    gpu_set_background(milton_state->render_data, background_color);
}

static void milton_set_default_view(CanvasView* view)
{
    *view = CanvasView{};

    view->scale               = MILTON_DEFAULT_SCALE;
    view->downsampling_factor = 1;
    view->num_layers          = 1;
    view->canvas_radius_limit = 1 << 30;  // A higher limit and certain assumptions start to break

}

static void milton_update_brushes(MiltonState* milton_state)
{
    for (int i = 0; i < BrushEnum_COUNT; ++i )
    {
        Brush* brush = &milton_state->brushes[i];
        i32 size = milton_state->brush_sizes[i];
        brush->radius = size * milton_state->view->scale;
        mlt_assert(brush->radius < FLT_MAX);
        if (i == BrushEnum_PEN)
        {
            // Alpha is set by the UI
            brush->color = to_premultiplied(gui_get_picker_rgb(milton_state->gui), brush->alpha);
        }
        else if (i == BrushEnum_ERASER)
        {
            brush->color = k_eraser_color;
        }
    }

    milton_state->working_stroke.brush = milton_state->brushes[BrushEnum_PEN];
}

static Brush milton_get_brush(MiltonState* milton_state)
{
    Brush brush = { 0 };
    if ( milton_state->current_mode == MiltonMode_PEN )
    {
        brush = milton_state->brushes[BrushEnum_PEN];
    }
    else if ( milton_state->current_mode == MiltonMode_ERASER )
    {
        brush = milton_state->brushes[BrushEnum_ERASER];
    }
    return brush;
}

static i32* pointer_to_brush_size(MiltonState* milton_state)
{
    i32* ptr = NULL;

    if ( milton_state->current_mode == MiltonMode_PEN )
    {
        ptr = &milton_state->brush_sizes[BrushEnum_PEN];
    }
    else if ( milton_state->current_mode == MiltonMode_ERASER )
    {
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

static void clear_stroke_redo(MiltonState* milton_state)
{
    while (milton_state->stroke_graveyard.count > 0)
    {
        Stroke s = pop(&milton_state->stroke_graveyard);
        mlt_free(s.points);
        mlt_free(s.pressures);
    }
    for (u64 i = 0; i < milton_state->redo_stack.count; ++i)
    {
        HistoryElement h = milton_state->redo_stack.data[i];
        if (h.type == HistoryElement_STROKE_ADD)
        {
            for (u64 j = i; j < milton_state->redo_stack.count-1; ++j)
            {
                milton_state->redo_stack.data[j] = milton_state->redo_stack.data[j+1];
            }
            pop(&milton_state->redo_stack);
        }
    }
}

static void milton_stroke_input(MiltonState* milton_state, MiltonInput* input)
{
    i64 num_discarded = 0;
    if ( input->input_count == 0 )
    {
        return;
    }

    Stroke* ws = &milton_state->working_stroke;

    //milton_log("Stroke input with %d packets\n", input->input_count);
    ws->brush    = milton_get_brush(milton_state);
    ws->layer_id = milton_state->view->working_layer_id;

    for (int input_i = 0; input_i < input->input_count; ++input_i)
    {
        v2i in_point = input->points[input_i];

        v2i canvas_point = raster_to_canvas(milton_state->view, in_point);

        f32 pressure = NO_PRESSURE_INFO;

        if (input->pressures[input_i] != NO_PRESSURE_INFO)
        {
            f32 pressure_min = 0.01f;
            pressure = pressure_min + input->pressures[input_i] * (1.0f - pressure_min);
            milton_state->flags |= MiltonStateFlags_STROKE_IS_FROM_TABLET;
        }

        // We haven't received pressure info, assume mouse input
        if (input->pressures[input_i] == NO_PRESSURE_INFO
            && !(milton_state->flags & MiltonStateFlags_STROKE_IS_FROM_TABLET))
        {
            pressure = 1.0f;
        }

        b32 not_the_first = false;
        if (ws->num_points >= 1)
        {
            not_the_first = true;
        }

        // A point passes inspection if:
        //  a) it's the first point of this stroke
        //  b) it is being appended to the stroke and it didn't merge with the previous point.
        b32 passed_inspection = true;

        if (pressure == NO_PRESSURE_INFO)
        {
            passed_inspection = false;
            num_discarded++;
        }

        if (passed_inspection && not_the_first)
        {
            i32 in_radius = (i32)(pressure * ws->brush.radius);

            // Limit the number of points we check so that we don't mess with the stroke too much.
            int point_window = 4;
            int count = 0;
            // Pop every point that is contained by the new one, but don't leave it empty

            for ( i32 i = ws->num_points - 1; passed_inspection && i >= 0; --i )
            {
                if (++count >= point_window)
                {
                    break;
                }
                v2i this_point = ws->points[i];
                i32 this_radius = (i32)(ws->brush.radius * ws->pressures[i]);

                if (stroke_point_contains_point(canvas_point, in_radius, this_point, this_radius))
                {
                    if (ws->num_points > 1)
                    {
                        --ws->num_points;
                    }
                    else
                    {
                        break;
                    }
                }
                else if (stroke_point_contains_point(this_point, this_radius, canvas_point, in_radius))
                {
                    // If some other point in the past contains this point,
                    // then this point is invalid.
                    passed_inspection = false;
                    num_discarded += ws->num_points - i;
                    break;
                }
            }
        }


        // Cleared to be appended.
        if (passed_inspection && ws->num_points < STROKE_MAX_POINTS-1)
        {
            // TODO: Add interpolation points here based on angle between consecutive points.
            // Once that's added, enable mouse input smoothing.

            if (milton_brush_smoothing_enabled(milton_state))
            {
                // Stroke smoothing.
                // Change canvas_point depending on the average of the last `N` points.
                // The new point is a weighted sum of factor*average (1-factor)*canvas_point
                i64 N = 2;
                if (ws->num_points > N && (milton_state->flags & MiltonStateFlags_STROKE_IS_FROM_TABLET))
                {
                    v2i average = {};
                    float factor = 0.55f;

                    // TODO: possible overflow.
                    //  Not for N=2? Canvas is [-2^30, 2^30]
                    for (i64 i = 0; i < N; ++i)
                    {
                        average.x += ws->points[ws->num_points-1 - i].x;
                        average.y += ws->points[ws->num_points-1 - i].y;
                    }
                    average.x /= N;
                    average.y /= N;

                    auto* view = milton_state->view;

                    auto canvas_center = raster_to_canvas(view, view->screen_center);

                    float f_average_x = average.x - canvas_center.x;
                    float f_average_y = average.y - canvas_center.y;

                    float f_canvas_point_x = canvas_point.x - canvas_center.x;
                    float f_canvas_point_y = canvas_point.y - canvas_center.y;

                    canvas_point.x = (i32)roundf
                            (f_average_x*factor +
                             f_canvas_point_x*(1-factor));
                    canvas_point.y = (i32)roundf
                            (f_average_y*factor +
                             f_canvas_point_y*(1-factor));

                    canvas_point.x += canvas_center.x;
                    canvas_point.y += canvas_center.y;
                }
            }

            // Add to current stroke.
            int index = ws->num_points++;
            ws->points[index] = canvas_point;
            ws->pressures[index] = pressure;
        }
    }

#if 0
    if (num_discarded > 0)
    {
        milton_log("INFO: Discarded %d points.\n", num_discarded);
    }
#endif

    // Validate. remove points that are standing in the same place, even if they have different pressures.
    for ( i32 np = 0; np < ws->num_points-1; ++np )
    {
        if ( equ2i(ws->points[np], ws->points[np+1]))
        {
            for ( i32 new_i = np; new_i < ws->num_points-1; ++new_i )
            {
                ws->points[new_i] = ws->points[new_i+1];
            }
            --ws->num_points;
        }
    }
}

void milton_set_canvas_file_(MiltonState* milton_state, PATH_CHAR* fname, b32 is_default)
{
    if (is_default)
    {
        milton_state->flags |= MiltonStateFlags_DEFAULT_CANVAS;
    }
    else
    {
        milton_state->flags &= ~MiltonStateFlags_DEFAULT_CANVAS;
    }

    if (milton_state->mlt_file_path != NULL)
    {
        mlt_free(milton_state->mlt_file_path);
    }

    u64 len = PATH_STRLEN(fname);
    if (len > MAX_PATH)
    {
        milton_log("milton_set_canvas_file: fname was too long %lu\n", len);
        fname = TO_PATH_STR("MiltonPersist.mlt");
    }
    milton_state->mlt_file_path = fname;

    if (!is_default)
    {
        milton_set_last_canvas_fname(fname);
    }
    else
    {
        milton_unset_last_canvas_fname();
    }
}

void milton_set_canvas_file(MiltonState* milton_state, PATH_CHAR* fname)
{
    milton_set_canvas_file_(milton_state, fname, false);
}

// Helper function
void milton_set_default_canvas_file(MiltonState* milton_state)
{
    PATH_CHAR* f = (PATH_CHAR*)mlt_calloc(MAX_PATH, sizeof(*f));
    PATH_STRNCPY(f, TO_PATH_STR("MiltonPersist.mlt"), MAX_PATH);
    platform_fname_at_config(f, MAX_PATH);
    milton_set_canvas_file_(milton_state, f, true);
    milton_state->flags |= MiltonStateFlags_DEFAULT_CANVAS;
}

void milton_gl_backend_draw(MiltonState* milton_state)
{
    MiltonGLState* gl = milton_state->gl;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl->texture);
    u8* raster_buffer = milton_state->raster_buffer;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 milton_state->view->screen_size.w, milton_state->view->screen_size.h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)raster_buffer);

    GLCHK (glUseProgram(gl->quad_program));
#if MILTON_USE_VAO
    glBindVertexArray(gl->quad_vao);
#else
    GLint pos_loc     = glGetAttribLocation(gl->quad_program, "position");
    GLint sampler_loc = glGetUniformLocation(gl->quad_program, "raster_buffer");
    mlt_assert (pos_loc     >= 0);
    mlt_assert (sampler_loc >= 0);
    GLCHK (glUniform1i(sampler_loc, 0 /*GL_TEXTURE0*/));
    GLCHK (glBindBuffer(GL_ARRAY_BUFFER, milton_state->gl->vbo) );
    GLCHK (glVertexAttribPointer(/*attrib location*/(GLuint)pos_loc,
                                 /*size*/2, GL_FLOAT,
                                 /*normalize*/ GL_FALSE,
                                 /*stride*/0,
                                 /*ptr*/0));

    GLCHK (glEnableVertexAttribArray((GLuint)pos_loc));
#endif
    GLCHK (glDrawArrays (GL_TRIANGLE_FAN, 0, 4) );
}

i32 milton_get_brush_size(MiltonState* milton_state)
{
    i32 brush_size = 0;
    if ( milton_state->current_mode == MiltonMode_PEN )
    {
        brush_size = milton_state->brush_sizes[BrushEnum_PEN];
    }
    else if ( milton_state->current_mode == MiltonMode_ERASER )
    {
        brush_size = milton_state->brush_sizes[BrushEnum_ERASER];
    }
    return brush_size;
}

void milton_set_brush_size(MiltonState* milton_state, i32 size)
{
    if ( current_mode_is_for_painting(milton_state) )
    {
        if ( size < MILTON_MAX_BRUSH_SIZE && size > 0 )
        {
            (*pointer_to_brush_size(milton_state)) = size;
            milton_update_brushes(milton_state);
        }
    }
    milton_state->flags |= MiltonStateFlags_BRUSH_SIZE_CHANGED;
}

// For keyboard shortcut.
void milton_increase_brush_size(MiltonState* milton_state)
{
    if ( current_mode_is_for_painting(milton_state) )
    {
        i32 brush_size = milton_get_brush_size(milton_state);
        if (brush_size < MILTON_MAX_BRUSH_SIZE && brush_size > 0)
        {
            milton_set_brush_size(milton_state, brush_size + 1);
        }
        milton_update_brushes(milton_state);
    }
}

// For keyboard shortcut.
void milton_decrease_brush_size(MiltonState* milton_state)
{
    if ( current_mode_is_for_painting(milton_state) )
    {
        i32 brush_size = milton_get_brush_size(milton_state);
        if (brush_size > 1)
        {
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


void milton_init(MiltonState* milton_state, i32 width, i32 height)
{
    init_localization();
    // Initialize render queue
    milton_state->render_stack = arena_alloc_elem(milton_state->root_arena, RenderStack);
    {
        milton_state->render_stack->work_available      = SDL_CreateSemaphore(0);
        milton_state->render_stack->completed_semaphore = SDL_CreateSemaphore(0);
        milton_state->render_stack->mutex               = SDL_CreateMutex();
    }

#if MILTON_SAVE_ASYNC
    milton_state->save_mutex = SDL_CreateMutex();
    milton_state->save_flag = SaveEnum_GOOD_TO_GO;
#endif

    milton_state->num_render_workers = min(SDL_GetCPUCount(), MAX_NUM_WORKERS);
#if RESTRICT_NUM_WORKERS_TO_2
    milton_state->num_render_workers = min(2, SDL_GetCPUCount());
#endif

    milton_log("[DEBUG]: Creating %d render workers.\n", milton_state->num_render_workers);

    milton_state->render_worker_arenas = arena_alloc_array(milton_state->root_arena,
                                                           milton_state->num_render_workers,
                                                           Arena);
    milton_state->worker_memory_size = 65536;


    mlt_assert (milton_state->num_render_workers);

    milton_state->bytes_per_pixel = 4;

    milton_state->working_stroke.points    = arena_alloc_array(milton_state->root_arena, STROKE_MAX_POINTS, v2i);
    milton_state->working_stroke.pressures = arena_alloc_array(milton_state->root_arena, STROKE_MAX_POINTS, f32);
    // Make the working stroke visible
    for (int wi=0; wi< milton_state->num_render_workers; ++wi)
    {
        milton_state->working_stroke.visibility[wi] = true;
    }


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

    milton_state->gui = arena_alloc_elem(milton_state->root_arena, MiltonGui);
    milton_state->render_data = arena_alloc_elem(milton_state->root_arena, RenderData);
    gui_init(milton_state->root_arena, milton_state->gui);

    milton_gl_backend_init(milton_state);

    milton_state->view = arena_alloc_elem(milton_state->root_arena, CanvasView);
    milton_set_default_view(milton_state->view);

    milton_state->view->screen_size = { width, height };

    // TODO: Resolve this circular dependency between gpu_init and CanvasView.background_color
    gpu_init(milton_state->render_data, milton_state->view);

    milton_set_background_color(milton_state, v3f{ 1, 1, 1 });


#if 0
    { // Get/Set Milton Canvas (.mlt) file


        PATH_CHAR* last_fname = milton_get_last_canvas_fname();

        if (last_fname != NULL)
        {
            milton_set_canvas_file(milton_state, last_fname);
        }
        else
        {
            milton_set_default_canvas_file(milton_state);
        }
    }
#else
    milton_set_default_canvas_file(milton_state);
#endif

    // Set default brush sizes.
    for (int i = 0; i < BrushEnum_COUNT; ++i)
    {
        switch (i)
        {
        case BrushEnum_PEN:
            milton_state->brush_sizes[i] = 10;
            break;
        case BrushEnum_ERASER:
            milton_state->brush_sizes[i] = 40;
            break;
        default:
            INVALID_CODE_PATH;
            break;
        }
    }
    milton_set_pen_alpha(milton_state, 1.0f);

    milton_state->mlt_binary_version = 2;
    milton_state->last_save_time = {};
    // Note: This will fill out uninitialized data like default layers.
    milton_load(milton_state);

    // Enable brush smoothing by default
    if (!milton_brush_smoothing_enabled(milton_state))
    {
        milton_toggle_brush_smoothing(milton_state);
    }

#if MILTON_DEBUG
    milton_run_tests(milton_state);
#endif
#if MILTON_ENABLE_PROFILING
    milton_state->viz_window_visible = true;
#endif

    for (i32 i = 0; i < milton_state->num_render_workers; ++i)
    {
        WorkerParams* params = arena_alloc_elem(milton_state->root_arena, WorkerParams);
        {
            params->milton_state = milton_state;
            params->worker_id = i;
        }
        mlt_assert (milton_state->render_worker_arenas[i].ptr == NULL);
        u8* worker_memory = (u8*)mlt_calloc(1, milton_state->worker_memory_size);
        if ( !worker_memory )
        {
            milton_die_gracefully("Platform allocation failed");
        }
        milton_state->render_worker_arenas[i] = arena_init(worker_memory,
                                                           milton_state->worker_memory_size);

        SDL_CreateThread(renderer_worker_thread, "Milton Render Worker", (void*)params);
    }

    milton_state->flags |= MiltonStateFlags_RUNNING;

#if MILTON_ENABLE_PROFILING
    profiler_init();
#endif


}

// Returns false if the pan_delta moves the pan vector outside of the canvas.
b32 milton_resize_and_pan(MiltonState* milton_state, v2i pan_delta, v2i new_screen_size)
{
    b32 pan_ok = true;
    if ((new_screen_size.w > 8000 ||
         new_screen_size.h > 8000 ||
         new_screen_size.w <= 0 ||
         new_screen_size.h <= 0) )
    {
        return pan_ok;
    }

    b32 do_realloc = false;
    if (milton_state->max_width <= new_screen_size.w)
    {
        milton_state->max_width = new_screen_size.w + 256;
        do_realloc = true;
    }
    if (milton_state->max_height <= new_screen_size.h)
    {
        milton_state->max_height = new_screen_size.h + 256;
        do_realloc = true;
    }

    size_t buffer_size = (size_t) milton_state->max_width * milton_state->max_height * milton_state->bytes_per_pixel;

    if (do_realloc)
    {
        u8* raster_buffer = milton_state->raster_buffer;
        u8* canvas_buffer = milton_state->canvas_buffer;
        if ( raster_buffer ) mlt_free(raster_buffer);
        if ( canvas_buffer ) mlt_free(canvas_buffer);
        milton_state->raster_buffer      = (u8*)mlt_calloc(1, buffer_size);
        milton_state->canvas_buffer      = (u8*)mlt_calloc(1, buffer_size);

        if (milton_state->raster_buffer == NULL)
        {
            milton_die_gracefully("Could not allocate enough memory for raster buffer.");
        }
        if (milton_state->canvas_buffer == NULL)
        {
            milton_die_gracefully("Could not allocate enough memory for canvas buffer.");
        }
    }

    if (new_screen_size.w < milton_state->max_width && new_screen_size.h < milton_state->max_height)
    {
        milton_state->view->screen_size = new_screen_size;
        milton_state->view->screen_center = divide2i(milton_state->view->screen_size, 2);

        // Add delta to pan vector
        v2i pan_vector = add2i(milton_state->view->pan_vector, (scale2i(pan_delta, milton_state->view->scale)));

        if (pan_vector.x > milton_state->view->canvas_radius_limit || pan_vector.x <= -milton_state->view->canvas_radius_limit)
        {
            pan_vector.x = milton_state->view->pan_vector.x;
            pan_ok = false;
        }
        if (pan_vector.y > milton_state->view->canvas_radius_limit || pan_vector.y <= -milton_state->view->canvas_radius_limit)
        {
            pan_vector.y = milton_state->view->pan_vector.y;
            pan_ok = false;
        }
        milton_state->view->pan_vector = pan_vector;

        // Upload data to gpu
        gpu_set_canvas(milton_state->render_data, milton_state->view);
    }
    else
    {
        milton_die_gracefully("Fatal error. Screen size is more than Milton can handle.");
    }

    gpu_resize(milton_state->render_data, milton_state->view);

    return pan_ok;
}

void milton_reset_canvas_and_set_default(MiltonState* milton_state)
{
    milton_state->mlt_binary_version = MILTON_MINOR_VERSION;
    Layer* l = milton_state->root_layer;
    while ( l != NULL )
    {
        for (u64 si = 0; si < l->strokes.count; ++si)
        {
            stroke_free(&l->strokes.data[si]);
        }
        release(&l->strokes);
        mlt_free(l->name);

        Layer* next = l->next;
        mlt_free(l);
        l = next;
    }
    milton_state->last_save_time = {};

    milton_state->layer_guid = 0;
    milton_state->root_layer = NULL;
    milton_state->working_layer = NULL;
    // New Root
    milton_new_layer(milton_state);

    // Clear history
    reset(&milton_state->history);
    reset(&milton_state->redo_stack);
    reset(&milton_state->stroke_graveyard);

    // New View
    milton_set_default_view(milton_state->view);

    // Reset color buttons
    for (ColorButton* b = &milton_state->gui->picker.color_buttons; b!=NULL; b=b->next)
    {
        b->rgba = {};
    }

    // gui init
    {
        MiltonGui* gui = milton_state->gui;
        gui->picker.data.hsv = { 0.0f, 1.0f, 0.7f };
        gui->visible = true;

        picker_init(&gui->picker);

        gui->preview_pos      = v2i{-1, -1};
        gui->preview_pos_prev = v2i{-1, -1};

        exporter_init(&gui->exporter);

        // TODO: Check if this line can be removed after the switch to HW rendering.
        milton_state->gui->flags |= MiltonGuiFlags_NEEDS_REDRAW;
    }
    milton_update_brushes(milton_state);
    milton_set_default_canvas_file(milton_state);
}

void milton_switch_mode(MiltonState* milton_state, MiltonMode mode)
{
    if (mode != milton_state->current_mode)
    {
        milton_state->last_mode = milton_state->current_mode;
        milton_state->current_mode = mode;

        if ( mode == MiltonMode_EXPORTING && milton_state->gui->visible)
        {
            gui_toggle_visibility(milton_state->gui);
        }
        if ( milton_state->last_mode == MiltonMode_EXPORTING && !milton_state->gui->visible)
        {
            gui_toggle_visibility(milton_state->gui);
        }
    }
}

void milton_use_previous_mode(MiltonState* milton_state)
{
    if (milton_state->last_mode != MiltonMode_NONE)
    {
        milton_switch_mode(milton_state, milton_state->last_mode);
    }
    else
    {
        INVALID_CODE_PATH;
    }
}

void milton_try_quit(MiltonState* milton_state)
{
    milton_state->flags &= ~MiltonStateFlags_RUNNING;
}

void milton_expand_render_memory(MiltonState* milton_state)
{
    if (milton_state->flags & MiltonStateFlags_WORKER_NEEDS_MEMORY)
    {
        size_t prev_memory_value = milton_state->worker_memory_size;
        milton_state->worker_memory_size *= 2;
        size_t needed_size = milton_state->worker_memory_size;

        for (int i = 0; i < milton_state->num_render_workers; ++i)
        {
            if (milton_state->render_worker_arenas[i].ptr != NULL)
            {
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

        milton_state->flags &= ~MiltonStateFlags_WORKER_NEEDS_MEMORY;
    }
}

static void milton_save_postlude(MiltonState* milton_state)
{
    milton_state->last_save_time = platform_get_walltime();
    milton_state->last_save_stroke_count = count_strokes(milton_state->root_layer);

    milton_state->flags &= ~MiltonStateFlags_LAST_SAVE_FAILED;
}

#if MILTON_SAVE_ASYNC
int  // Thread
milton_save_async(void* state_)
{
    MiltonState* milton_state = state_;

    SDL_LockMutex(milton_state->save_mutex);
    i64 flag = milton_state->save_flag;

    if(flag == SaveEnum_GOOD_TO_GO)
    {
        milton_state->save_flag = SaveEnum_IN_USE;
        SDL_UnlockMutex(milton_state->save_mutex);

        milton_save(milton_state);

        SDL_Delay(2000);  // Overkill to save more than every two seconds!.

        SDL_LockMutex(milton_state->save_mutex);
        milton_state->save_flag = SaveEnum_GOOD_TO_GO;
        SDL_UnlockMutex(milton_state->save_mutex);
    }
    else if (flag == SaveEnum_IN_USE)
    {
        SDL_UnlockMutex(milton_state->save_mutex);
    }

    return flag;
}
#endif

void milton_new_layer(MiltonState* milton_state)
{
    i32 id = milton_state->layer_guid++;
    milton_log("Increased guid to %d\n", milton_state->layer_guid);

    Layer* layer = (Layer*)mlt_calloc(1, sizeof(Layer));
    {
        layer->id = id;
        layer->name = (char*)mlt_calloc(MAX_LAYER_NAME_LEN, sizeof(char));
        layer->flags = LayerFlags_VISIBLE;
    }
    snprintf(layer->name, 1024, "Layer %d", layer->id);

    if (milton_state->root_layer != NULL)
    {
        Layer* top = layer_get_topmost(milton_state->root_layer);
        top->next = layer;
        layer->prev = top;
        milton_set_working_layer(milton_state, top->next);
    }
    else
    {
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
    if (layer->next || layer->prev)
    {
        if (layer->next) layer->next->prev = layer->prev;
        if (layer->prev) layer->prev->next = layer->next;

        Layer* wl = NULL;
        if (layer->next) wl = layer->next;
        else wl = layer->prev;
        milton_set_working_layer(milton_state, wl);
    }
    if ( layer == milton_state->root_layer )
        milton_state->root_layer = milton_state->working_layer;
    mlt_free(layer);
    milton_state->flags |= MiltonStateFlags_REQUEST_QUALITY_REDRAW;
}

static b32 milton_brush_smoothing_enabled(MiltonState* milton_state)
{
    b32 enabled = (milton_state->flags & MiltonStateFlags_BRUSH_SMOOTHING);
    return enabled;
}

static void milton_toggle_brush_smoothing(MiltonState* milton_state)
{
    if (milton_brush_smoothing_enabled(milton_state))
    {
        milton_state->flags &= ~MiltonStateFlags_BRUSH_SMOOTHING;
    }
    else
    {
        milton_state->flags |= MiltonStateFlags_BRUSH_SMOOTHING;
    }
}

static void milton_validate(MiltonState* milton_state)
{
    // Make sure that the history reflects the strokes that exist
    i64 num_layers=0;
    for(Layer* l = milton_state->root_layer; l != NULL; l = l->next)
    {
        ++num_layers;
    }
    i32* layer_ids = (i32*)mlt_calloc((size_t)num_layers, sizeof(i32));
    i64 i = 0;
    for(Layer* l = milton_state->root_layer; l != NULL; l = l->next)
    {
        layer_ids[i] = l->id;
        ++i;
    }

    i64 history_count = 0;
    for (u64 hi = 0; hi < milton_state->history.count; ++hi)
    {
        i32 id = milton_state->history.data[hi].layer_id;
        for (i64 li = 0; li < num_layers; ++li)
        {
            if (id == layer_ids[li])
            {
                ++history_count;
            }
        }
    }

    i64 stroke_count = count_strokes(milton_state->root_layer);
    if (history_count != stroke_count)
    {
        milton_log("Recreating history. File says History: %d(max %d) Actual strokes: %d\n",
                   history_count, milton_state->history.count,
                   stroke_count);
        reset(&milton_state->history);
        for(Layer *l = milton_state->root_layer;
            l != NULL;
            l = l->next)
        {
            for (u64 si = 0; si < l->strokes.count; ++si)
            {
                Stroke* s = l->strokes.data + si;
                HistoryElement he = { HistoryElement_STROKE_ADD, s->layer_id };
                push(&milton_state->history, he);
            }
        }
    }

    mlt_free(layer_ids);
}

void milton_update(MiltonState* milton_state, MiltonInput* input)
{
    PROFILE_GRAPH_BEGIN(update);
    b32 should_save =
            ((input->flags & MiltonInputFlags_OPEN_FILE)) ||
            ((input->flags & MiltonInputFlags_SAVE_FILE)) ||
            ((input->flags & MiltonInputFlags_END_STROKE)) ||
            ((input->flags & MiltonInputFlags_UNDO)) ||
            ((input->flags & MiltonInputFlags_REDO));

    //MiltonRenderFlags
    int render_flags = MiltonRenderFlags_NONE;

    if (!(milton_state->flags & MiltonStateFlags_RUNNING))
    {
        // Someone tried to kill milton from outside the update. Make sure we save.
        should_save = true;
        goto cleanup;
    }

    if (input->flags & MiltonInputFlags_OPEN_FILE)
    {
        milton_load(milton_state);
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
        input->flags |= MiltonInputFlags_FAST_DRAW;
    }


    if (milton_state->flags & MiltonStateFlags_WORKER_NEEDS_MEMORY)
    {
        milton_expand_render_memory(milton_state);
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
    }

    if (milton_state->flags & MiltonStateFlags_REQUEST_QUALITY_REDRAW)
    {
        milton_state->view->downsampling_factor = 1;  // See how long it takes to redraw at full quality
        milton_state->flags &= ~MiltonStateFlags_REQUEST_QUALITY_REDRAW;
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
    }

    i32 now = (i32)SDL_GetTicks();

    if ((input->flags & MiltonInputFlags_FAST_DRAW))
    {
        render_flags |= MiltonRenderFlags_DRAW_ITERATIVELY;
        milton_state->quality_redraw_time = now;
    }
    else if (milton_state->quality_redraw_time > 0 &&
              (now - milton_state->quality_redraw_time) > QUALITY_REDRAW_TIMEOUT_MS)
    {
        milton_state->flags |= MiltonStateFlags_REQUEST_QUALITY_REDRAW;  // Next update loop.
        milton_state->quality_redraw_time = 0;
    }

    if (input->flags & MiltonInputFlags_FULL_REFRESH)
    {
        render_flags |= MiltonRenderFlags_FULL_REDRAW;
    }

    if (input->scale)
    {
        render_flags |= MiltonRenderFlags_FULL_REDRAW;

// Debug
#if MILTON_ZOOM_DEBUG
        f32 scale_factor = 1.5f;
        i32 view_scale_limit = 1 << 20;
// Sensible
#else
        f32 scale_factor = 1.3f;
        i32 view_scale_limit = (1 << 13);
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

        if (input->scale > 0 && milton_state->view->scale >= min_scale)
        {
            milton_state->view->scale = (i32)(ceilf(milton_state->view->scale / scale_factor));
        }
        else if (input->scale < 0 && milton_state->view->scale < view_scale_limit)
        {
            milton_state->view->scale = (i32)(milton_state->view->scale * scale_factor) + 1;
        }
        milton_update_brushes(milton_state);
        gpu_update_scale(milton_state->render_data, milton_state->view->scale);
    }
    else if ((input->flags & MiltonInputFlags_PANNING))
    {
        // If we are *not* zooming and we are panning, we can copy most of the
        // framebuffer
        if (!equ2i(input->pan_delta, v2i{}))
        {
            render_flags |= MiltonRenderFlags_PAN_COPY;
        }
    }

    if ((input->flags & MiltonInputFlags_CHANGE_MODE))
    {
        MiltonMode mode = milton_state->current_mode;
        if (mode == input->mode_to_set)
        {
            // Modes we can toggle
            if (mode == MiltonMode_EYEDROPPER)
            {
                if (milton_state->last_mode != MiltonMode_EYEDROPPER)
                {
                    milton_use_previous_mode(milton_state);
                }
                else
                {
                    // This is not supposed to happen but if we get here we won't crash and burn.
                    milton_switch_mode(milton_state, MiltonMode_PEN);
                    milton_log("Warning: Unexpected code path: Toggling modes. Eye dropper was set *twice*. Switching to pen.");
                }
            }
        }
        else
        {
            // Change the current mode if it's different from the current mode.
            milton_switch_mode( milton_state, input->mode_to_set );
            if (input->mode_to_set == MiltonMode_PEN ||
                 input->mode_to_set == MiltonMode_ERASER)
            {
                milton_update_brushes(milton_state);
            }
        }

        render_flags |= MiltonRenderFlags_UI_UPDATED;
    }

    { // Undo / Redo
        if ((input->flags & MiltonInputFlags_UNDO))
        {
            // Grab undo elements. They might be from deleted layers, so discard dead results.
            while (milton_state->history.count > 0)
            {
                HistoryElement h = pop(&milton_state->history);
                Layer* l = layer_get_by_id(milton_state->root_layer, h.layer_id);
                if (l)
                { // found a thing to undo.
                    if (l->strokes.count > 0)
                    {
                        Stroke stroke = pop(&l->strokes);
                        push(&milton_state->stroke_graveyard, stroke);
                        push(&milton_state->redo_stack, h);
                        // TODO: FULL_REDRAW is overkill
                        render_flags |= MiltonRenderFlags_FULL_REDRAW;
                    }
                    break;
                }
            }
        }
        else if ((input->flags & MiltonInputFlags_REDO))
        {
            if (milton_state->redo_stack.count > 0)
            {
                HistoryElement h = pop(&milton_state->redo_stack);
                switch (h.type)
                {
                case HistoryElement_STROKE_ADD:
                    {
                    Layer* l = layer_get_by_id(milton_state->root_layer, h.layer_id);
                    if (l)
                    {
                        Stroke stroke = pop(&milton_state->stroke_graveyard);
                        if (stroke.layer_id == h.layer_id)
                        {
                            push(&l->strokes, stroke);
                            push(&milton_state->history, h);
                            // TODO: FULL_REDRAW is overkill
                            render_flags |= MiltonRenderFlags_FULL_REDRAW;
                            break;
                        }
                        stroke = pop(&milton_state->stroke_graveyard);  // Keep popping in case the graveyard has info from deleted layers
                    }

                    } break;
                /* case HistoryElement_LAYER_DELETE: { */
                /* } break; */
                }
            }
        }
    }

    // If the current mode is Pen or Eraser, we show the hover. It can be unset under various conditions later.
    if (milton_state->current_mode == MiltonMode_PEN ||
        milton_state->current_mode == MiltonMode_ERASER)
    {
        render_flags |= MiltonRenderFlags_BRUSH_HOVER;
    }

    if ((input->flags & MiltonInputFlags_HOVERING))
    {
        milton_state->hover_point = input->hover_point;
        render_flags |= MiltonRenderFlags_BRUSH_HOVER;
    }

    if (milton_state->gui->active)
    {
        render_flags &= ~MiltonRenderFlags_BRUSH_HOVER;
    }

    if (is_user_drawing(milton_state))
    {  // Set the hover point as the last point of the working stroke.
        i32 np = milton_state->working_stroke.num_points;
        if (np > 0)
        {
            milton_state->hover_point = canvas_to_raster(milton_state->view, milton_state->working_stroke.points[np-1]);
        }
    }

    if (input->input_count > 0)
    {
        if (current_mode_is_for_painting(milton_state))
        {
            if (!is_user_drawing(milton_state) && picker_consume_input(milton_state->gui, input))
            {
                render_flags |= gui_process_input(milton_state, input);
                milton_update_brushes(milton_state);
            }
            else if (!milton_state->gui->active && (milton_state->working_layer->flags & LayerFlags_VISIBLE))
            {
                milton_stroke_input(milton_state, input);
            }
        }
    }

    if (milton_state->current_mode == MiltonMode_EXPORTING)
    {
        b32 changed = exporter_input(&milton_state->gui->exporter, input);
        if (changed)
        {
            render_flags |= MiltonRenderFlags_UI_UPDATED;
        }
    }

    if ((input->flags & MiltonInputFlags_IMGUI_GRABBED_INPUT))
    {
        // Start drawing the preview if we just grabbed a slider.
        render_flags &= ~MiltonRenderFlags_BRUSH_HOVER;
        if ((milton_state->gui->flags & MiltonGuiFlags_SHOWING_PREVIEW))
        {
            render_flags |= MiltonRenderFlags_BRUSH_PREVIEW;
        }
    }
    else
    {
        gui_imgui_set_ungrabbed(milton_state->gui);
    }

    if (milton_state->current_mode == MiltonMode_EYEDROPPER)
    {
        v2i point = {0};
        b32 in = false;
        if ((input->flags & MiltonInputFlags_CLICK))
        {
            point = input->click;
            in = true;
        }
        if ((input->flags & MiltonInputFlags_HOVERING))
        {
            point = input->hover_point;
            in = true;
        }
        if (in)
        {
            eyedropper_input(milton_state->gui, (u32*)milton_state->canvas_buffer,
                             milton_state->view->screen_size.w,
                             milton_state->view->screen_size.h,
                             point);
            render_flags |= MiltonRenderFlags_UI_UPDATED;
        }
        if(input->flags & MiltonInputFlags_CLICKUP)
        {
            if (!(milton_state->flags & MiltonStateFlags_IGNORE_NEXT_CLICKUP))
            {
                milton_switch_mode(milton_state, MiltonMode_PEN);
                milton_update_brushes(milton_state);
            }
            else
            {
                milton_state->flags &= ~MiltonStateFlags_IGNORE_NEXT_CLICKUP;
            }
        }
    }

    // ---- End stroke
    if ((input->flags & MiltonInputFlags_END_STROKE))
    {
        milton_state->flags &= ~MiltonStateFlags_STROKE_IS_FROM_TABLET;

        if (milton_state->gui->active)
        {
            gui_deactivate(milton_state->gui);
            render_flags |= MiltonRenderFlags_UI_UPDATED;
            render_flags &= ~MiltonRenderFlags_BRUSH_HOVER;
        }
        else
        {
            if (milton_state->working_stroke.num_points > 0)
            {
                // We used the selected color to draw something. Push.
                if (gui_mark_color_used(milton_state->gui))
                {
                    render_flags |= MiltonRenderFlags_UI_UPDATED;
                }
                // Copy current stroke.
                i32 num_points = milton_state->working_stroke.num_points;
                Stroke new_stroke = {};
                {
                    new_stroke.brush = milton_state->working_stroke.brush;
                    new_stroke.points = (v2i*)mlt_calloc((size_t)num_points, sizeof(v2i));
                    new_stroke.pressures = (f32*)mlt_calloc((size_t)num_points, sizeof(f32));
                    new_stroke.num_points = num_points;
                    new_stroke.layer_id = milton_state->view->working_layer_id;
                    memcpy(new_stroke.points, milton_state->working_stroke.points,
                           milton_state->working_stroke.num_points * sizeof(v2i));
                    memcpy(new_stroke.pressures, milton_state->working_stroke.pressures,
                           milton_state->working_stroke.num_points * sizeof(f32));
                    new_stroke.bounding_rect = bounding_box_for_stroke(&new_stroke);
                }

                auto* stroke = layer_push_stroke(milton_state->working_layer, new_stroke);
                gpu_add_stroke(milton_state->root_arena, milton_state->render_data, stroke);

                HistoryElement h = { HistoryElement_STROKE_ADD, milton_state->working_layer->id };
                push(&milton_state->history, h);
                // Clear working_stroke
                {
                    milton_state->working_stroke.num_points = 0;
                }

                clear_stroke_redo(milton_state);

                render_flags |= MiltonRenderFlags_FINISHED_STROKE;

            }
        }
    }

    // Disable hover if panning.
    if (input->flags & MiltonInputFlags_PANNING)
    {
        render_flags &= ~MiltonRenderFlags_BRUSH_HOVER;
    }

    // If the brush size was changed, set up the renderer
    if ((milton_state->flags & MiltonStateFlags_BRUSH_SIZE_CHANGED))
    {
        milton_state->flags &= ~MiltonStateFlags_BRUSH_SIZE_CHANGED;
        render_flags |= MiltonRenderFlags_BRUSH_CHANGE;
    }

    if ((milton_state->flags & MiltonStateFlags_BRUSH_HOVER_FLASHING))
    {  // Send a UI_UPDATED event to clear the canvas of the hover when it stops flashing
        if ((i32)SDL_GetTicks() - milton_state->hover_flash_ms > HOVER_FLASH_THRESHOLD_MS)
        {
            milton_state->flags &= ~MiltonStateFlags_BRUSH_HOVER_FLASHING;
            render_flags |= MiltonRenderFlags_UI_UPDATED;
        }
    }

    if ((milton_state->gui->flags & MiltonGuiFlags_NEEDS_REDRAW))
    {
        milton_state->gui->flags &= ~MiltonGuiFlags_NEEDS_REDRAW;
        render_flags |= MiltonRenderFlags_UI_UPDATED;
    }
    if ((milton_state->gui->flags & MiltonGuiFlags_SHOWING_PREVIEW))
    {
        render_flags |= MiltonRenderFlags_UI_UPDATED;
    }

    PROFILE_GRAPH_PUSH(update);
    // milton_render(milton_state, render_flags, input->pan_delta);

cleanup:
    if (!(milton_state->flags & MiltonStateFlags_RUNNING))
    {
        cursor_show();
    }
    if (should_save)
    {

        // If quitting, always be serial. Otherwise, be async.
        if (!(milton_state->flags & MiltonStateFlags_RUNNING))
        {
            // We want to block so that the main thread doesn't die before the async function finishes.
            milton_save(milton_state);
        } else {
#if MILTON_SAVE_ASYNC
            SDL_CreateThread(milton_save_async, "Async Save Thread", (void*)milton_state);
#else
            milton_save(milton_state);
#endif
        }
        // We're about to close and the last save failed and the drawing changed.
        if (!(milton_state->flags & MiltonStateFlags_RUNNING) &&
             (milton_state->flags & MiltonStateFlags_LAST_SAVE_FAILED) &&
             (milton_state->flags & MiltonStateFlags_MOVE_FILE_FAILED) &&
             milton_state->last_save_stroke_count != count_strokes(milton_state->root_layer))
        {
            // TODO: Stop using MoveFileEx?
            //  Why does MoveFileEx fail? Ask someone who knows this stuff.
            // Wait a moment and try again. If this fails, prompt to save somewhere else.
            SDL_Delay(3000);
            milton_save(milton_state);

            if ((milton_state->flags & MiltonStateFlags_LAST_SAVE_FAILED) &&
                (milton_state->flags & MiltonStateFlags_MOVE_FILE_FAILED))
            {
                char msg[1024];
                WallTime lst = milton_state->last_save_time;
                snprintf(msg, 1024, "Milton failed to save this canvas. The last successful save was at %.2d:%.2d:%.2d. Try saving to another file?",
                         lst.hours, lst.minutes, lst.seconds);
                b32 another = platform_dialog_yesno(msg, "Try another file?");
                if (another)
                {
                    // NOTE(possible refactor): There is similar code. Guipp.cpp save_milton_canvas
                    PATH_CHAR* name = platform_save_dialog(FileKind_MILTON_CANVAS);
                    if (name)
                    {
                        milton_log("Saving to %s\n", name);
                        milton_set_canvas_file(milton_state, name);
                        milton_save(milton_state);
                        if ( (milton_state->flags & MiltonStateFlags_LAST_SAVE_FAILED) )
                        {
                            platform_dialog("Still can't save. Please contact us for help. miltonpaint.com", "Info");
                        }
                        else
                        {
                            platform_dialog("Success.", "Info");
                        }
                        b32 del = platform_delete_file_at_config(TO_PATH_STR("MiltonPersist.mlt"), DeleteErrorTolerance_OK_NOT_EXIST);
                        if (del == false)
                        {
                            platform_dialog("Could not delete default canvas."
                                            " Contents will be still there when you create a new canvas.", "Info");
                        }
                    }
                }
            }
        }
    }
    profiler_output();

    //milton_validate(milton_state);
    ARENA_VALIDATE(milton_state->root_arena);
}

