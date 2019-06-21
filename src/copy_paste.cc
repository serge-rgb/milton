#define MAX_PASTA_POINTS (1 << 16)

void
pasta_init(Arena* a, CopyPaste* pasta)
{
    pasta->selection = arena_alloc_elem(a, Selection);
    pasta->selection->points = arena_alloc_array(a, MAX_PASTA_POINTS, v2f);
}

void
pasta_input(CopyPaste* pasta, MiltonInput* input)
{
    f32 min_distance = 20.0f;
    Selection* s = pasta->selection;
    for ( i64 input_i = 0; input_i < input->input_count; input_i++ ) {
        b32 add_point = false;

        v2f point = v2i_to_v2f(input->points[input_i]);

        if (s->num_points == 0) {
            add_point = true;
        }
        else if (distance(point, s->points[s->num_points-1]) > min_distance) {
            add_point = true;
        }

        if (add_point && s->num_points < MAX_PASTA_POINTS-1) {
            s->points[s->num_points++] = point;
        }
    }
}

void
pasta_clear(CopyPaste* pasta)
{
    pasta->selection->num_points = 0;
}