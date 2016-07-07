// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license



u8* arena_alloc_bytes(Arena* arena, size_t num_bytes)
{
    size_t total = arena->count + num_bytes;
    if (total > arena->size)
    {
        mlt_assert(!"Out of memory!");
        return NULL;
    }
    u8* result = arena->ptr + arena->count;
    arena->count += num_bytes;
    return result;
}

Arena arena_init(void* base, size_t size)
{
    Arena arena = { 0 };
    arena.ptr = (u8*)base;
    if (arena.ptr)
    {
        arena.size = size;
    }
    return arena;
}

Arena arena_spawn(Arena* parent, size_t size)
{
    u8* ptr = arena_alloc_bytes(parent, size);
    mlt_assert(ptr);

    Arena child = { 0 };
    {
        child.ptr    = ptr;
        child.size   = size;
    }

    return child;
}

Arena arena_push(Arena* parent, size_t size)
{
    mlt_assert ( size <= arena_available_space(parent));
    Arena child = { 0 };
    {
        child.parent = parent;
        child.id     = parent->num_children;
        u8* ptr = arena_alloc_bytes(parent, size);
        parent->num_children += 1;
        child.ptr = ptr;
        child.size = size;
    }
    return child;
}

void arena_pop(Arena* child)
{
    Arena* parent = child->parent;
    mlt_assert(parent);

    // Assert that this child was the latest push.
    mlt_assert ((parent->num_children - 1) == child->id);

    parent->count -= child->size;
    char* ptr = (char*)(parent->ptr) + parent->count;
    memset(ptr, 0, child->count);
    parent->num_children -= 1;
}

void   arena_pop_noclear(Arena* child)
{
    Arena* parent = child->parent;
    mlt_assert(parent);

    // Assert that this child was the latest push.
    mlt_assert ((parent->num_children - 1) == child->id);

    parent->count -= child->size;
    parent->num_children -= 1;
}

void arena_reset(Arena* arena)
{
    memset (arena->ptr, 0, arena->count);
    arena->count = 0;
}

void arena_reset_noclear(Arena* arena)
{
    arena->count = 0;
}
