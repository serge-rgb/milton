// memory.h
// (c) Copyright 2015 Sergio Gonzalez
//
// Released under the MIT license. See LICENSE.txt


#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Arena_s Arena;

struct Arena_s
{
    // Memory:
    size_t size;
    size_t count;
    int8_t* ptr;

    // For pushing/popping
    Arena*  parent;
    int     id;
    int     num_children;
};

// =========================================
// ==== Arena creation                  ====
// =========================================

// Create a root arena from a memory block.
static Arena arena_init(void* base, size_t size);
// Create a child arena.
static Arena arena_spawn(Arena* parent, size_t size);

// ==== Temporary arenas.
// Usage:
//      child = arena_push(my_arena, some_size);
//      use_temporary_arena(&child.arena);
//      arena_pop(child);
static Arena    arena_push(Arena* parent, size_t size);
static void     arena_pop (Arena* child);

// =========================================
// ====          Allocation             ====
// =========================================
#define      arena_alloc_elem(arena, T)         (T *)arena_alloc_bytes((arena), sizeof(T))
#define      arena_alloc_array(arena, count, T) (T *)arena_alloc_bytes((arena), (count) * sizeof(T))
static void* arena_alloc_bytes(Arena* arena, size_t num_bytes);

// =========================================
// ====        Utility                  ====
// =========================================

#define arena_available_space(arena)    ((arena)->size - (arena)->count)
#define ARENA_VALIDATE(arena)           assert ((arena)->num_children == 0)

// =========================================
// ====            Reuse                ====
// =========================================
static void arena_reset(Arena* arena);


// =========================================
//        Implementation
// =========================================

static void* arena_alloc_bytes(Arena* arena, size_t num_bytes)
{
    size_t total = arena->count + num_bytes;
    if (total > arena->size)
    {
        return NULL;
    }
    void* result = arena->ptr + arena->count;
    arena->count += num_bytes;
    return result;
}

static Arena arena_init(void* base, size_t size)
{
    Arena arena = { 0 };
    arena.ptr = (int8_t*)base;
    if (arena.ptr)
    {
        arena.size   = size;
    }
    return arena;
}

static Arena arena_spawn(Arena* parent, size_t size)
{
    void* ptr = arena_alloc_bytes(parent, size);
    assert(ptr);

    Arena child = { 0 };
    {
        child.ptr    = ptr;
        child.size   = size;
    }

    return child;
}

static Arena arena_push(Arena* parent, size_t size)
{
    assert ( size <= arena_available_space(parent));
    Arena child = { 0 };
    {
        child.parent = parent;
        child.id     = parent->num_children;
        void* ptr = arena_alloc_bytes(parent, size);
        parent->num_children += 1;
        child.ptr = ptr;
        child.size = size;
    }
    return child;
}

static void arena_pop(Arena* child)
{
    Arena* parent = child->parent;
    assert(parent);

    // Assert that this child was the latest push.
    assert ((parent->num_children - 1) == child->id);

    parent->count -= child->size;
    char* ptr = (char*)(parent->ptr) + parent->count;
    memset(ptr, 0, child->count);
    parent->num_children -= 1;

    *child = (Arena){ 0 };
}

static void arena_reset(Arena* arena)
{
    memset (arena->ptr, 0, arena->count);
    arena->count = 0;
}

#ifdef __cplusplus
}
#endif
