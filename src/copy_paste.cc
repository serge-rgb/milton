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
    f32 min_distance = 50.0f;
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
    b32 finish_selecting = input->flags & MiltonInputFlags_POINTER_RELEASE;

    // Check for self-intersection
    for (i32 i = 0; i < (i32)s->num_points - 1; ++i) {
        // Grab first segment
        v2f o1 = s->points[i];
        v2f d1 = s->points[i+1] - o1;

        for (i32 j = i+1; j < (i32)s->num_points - 1; ++j) {
            // Second segment
            v2f o2 = s->points[j];
            v2f d2 = s->points[j+1] - o2;

            // Intersect segments
            f32 disc = dot(d1, perp(d2));
            if (disc != 0.0f) {
                f32 t = dot((o2 - o1), perp(d2)) / disc;
                if (t > 0.0f && t < 1.0f) {
                    finish_selecting = true;
                    break;
                }
            }

        }
    }

    if (finish_selecting) {
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
