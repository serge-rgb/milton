void
pasta_init(Arena* a, CopyPaste* pasta)
{
    pasta->selection = arena_alloc_elem(a, Selection);
    pasta->selection->points = arena_alloc_array(a, 1<<16, v2l);
}

void
pasta_input(CopyPaste* pasta, MiltonInput* input)
{
    for ( i64 input_i = 0; input_i < input->input_count; input_i++ ) {
        v2i point = input->points[input_i];
    }
}