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
    f32 min_distance = 5.0f;
    Selection* s = pasta->selection;
    for ( i64 input_i = 0; input_i < input->input_count; input_i++ ) {

        v2f point = v2i_to_v2f(input->points[input_i]);

        b32 add_point = (s->num_points == 0) || (distance(point, s->points[s->num_points-1]) > min_distance);

        if (add_point) {
            switch (pasta->fsm) {
                case PastaFSM_READY:
                    s->num_points = 0;
                    // Fall through
                case PastaFSM_EMPTY:
                    pasta->fsm = PastaFSM_SELECTING;
                    // Fall through
                case PastaFSM_SELECTING: {
                    if (add_point && s->num_points < MAX_PASTA_POINTS-1) {
                        s->points[s->num_points++] = point;
                    }
                } break;
            }
        }
    }
    if (input->flags & MiltonInputFlags_POINTER_RELEASE) {
        switch (pasta->fsm) {
            case PastaFSM_EMPTY:
            case PastaFSM_READY:
            default: {
                // Nothing
            } break;
            case PastaFSM_SELECTING: {
                pasta->fsm = PastaFSM_READY;
            } break;
        }
    }
}

void
pasta_clear(CopyPaste* pasta)
{
    pasta->fsm = PastaFSM_EMPTY;
    pasta->selection->num_points = 0;
}