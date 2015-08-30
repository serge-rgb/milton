// memory.h
// (c) Copyright 2015 Sergio Gonzalez
//
// Released under the MIT license. See LICENSE.txt

typedef struct Arena_s Arena;

struct Arena_s
{
    // Memory:
    size_t  size;
    size_t  count;
    u8*     ptr;

    // For pushing/popping
    Arena*  parent;
    int     id;
    int     num_children;
};

// =========================================
// ==== Arena creation                  ====
// =========================================

// Create a root arena from a memory block.
func Arena arena_init(void* base, size_t size);
// Create a child arena.
func Arena arena_spawn(Arena* parent, size_t size);

// ==== Temporary arenas.
// Usage:
//      child = arena_push(my_arena, some_size);
//      use_temporary_arena(&child.arena);
//      arena_pop(child);
func Arena    arena_push(Arena* parent, size_t size);
func void     arena_pop (Arena* child);

// =========================================
// ====          Allocation             ====
// =========================================
#define      arena_alloc_elem(arena, T)         (T *)arena_alloc_bytes((arena), sizeof(T))
#define      arena_alloc_array(arena, count, T) (T *)arena_alloc_bytes((arena), (count) * sizeof(T))
func void* arena_alloc_bytes(Arena* arena, size_t num_bytes);

// =========================================
// ====        Utility                  ====
// =========================================

#define arena_available_space(arena)    ((arena)->size - (arena)->count)
#define ARENA_VALIDATE(arena)           assert ((arena)->num_children == 0)

// =========================================
// ====            Reuse                ====
// =========================================
func void arena_reset(Arena* arena);

// =========================================
// ====   Simple stack from aren        ====
// =========================================
#define arena_make_stack(a, size, type) (type *)arena__stack_typeless(a, sizeof(type) * size)
#define stack_push(a, e) if (arena__stack_try_grow(a)) a[arena__stack_header(a)->count - 1] = e
#define stack_reset(a) (arena__stack_header(a)->count = 0)
#define stack_count(a) (arena__stack_header(a)->count)

#pragma pack(push, 1)
typedef struct
{
    size_t size;
    size_t count;
} StackHeader;
#pragma pack(pop)

#define arena__stack_header(stack) ((StackHeader*)((uint8_t*)stack - sizeof(StackHeader)))

func void* arena__stack_typeless(Arena* arena, size_t size)
{
    Arena child = arena_spawn(arena, size + sizeof(StackHeader));
    StackHeader head = { 0 };
    {
        head.size = child.size;
    }
    memcpy(child.ptr, &head, sizeof(StackHeader));
    return (void*)(((uint8_t*)child.ptr) + sizeof(StackHeader));
}

func int arena__stack_try_grow(void* stack)
{
    StackHeader* head = arena__stack_header(stack);
    if (head->size < (head->count + 1))
    {
        assert (!"Stack full");
        return 0;
    }
    ++head->count;
    return 1;
}


// =========================================
//        Arena implementation
// =========================================

func void* arena_alloc_bytes(Arena* arena, size_t num_bytes)
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

func Arena arena_init(void* base, size_t size)
{
    Arena arena = { 0 };
    arena.ptr = (u8*)base;
    if (arena.ptr)
    {
        arena.size = size;
    }
    return arena;
}

func Arena arena_spawn(Arena* parent, size_t size)
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

func Arena arena_push(Arena* parent, size_t size)
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

func void arena_pop(Arena* child)
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

func void arena_reset(Arena* arena)
{
    memset (arena->ptr, 0, arena->count);
    arena->count = 0;
}

