void
pasta_init(Arena* a, CopyPaste* pasta)
{
    pasta->selection = arena_alloc_elem(a, Selection);
    pasta->selection->points = arena_alloc_array(a, 1<<16, v2l);
}
