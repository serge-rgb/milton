//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


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

// Create a root arena from a memory block.
func Arena arena_init(void* base, size_t size);
func Arena arena_spawn(Arena* parent, size_t size);
func void  arena_reset(Arena* arena);

// ==== Temporary arenas.
// Usage:
//      child = arena_push(my_arena, some_size);
//      use_temporary_arena(&child.arena);
//      arena_pop(child);
func Arena  arena_push(Arena* parent, size_t size);
func void   arena_pop (Arena* child);

#define     arena_alloc_elem(arena, T)          (T *)arena_alloc_bytes((arena), sizeof(T))
#define     arena_alloc_array(arena, count, T)  (T *)arena_alloc_bytes((arena), (count) * sizeof(T))
#define     arena_available_space(arena)        ((arena)->size - (arena)->count)
#define     ARENA_VALIDATE(arena)               assert ((arena)->num_children == 0)


func void* arena_alloc_bytes(Arena* arena, size_t num_bytes);

// =======================
// == Dynamic allocator ==
// =======================
//
// Notes:
//  Not type safe. It's a poor man's malloc for the few uses in Milton where we
//  need dynamic allocation:
//
// List of places in Milton that need dynamic allocation:
//
//  - Render worker memory. Ever growing, No realloc with simple arenas.
//  - Milton resize window

#define     dyn_alloc(T, n)     (T*)dyn_alloc_typeless(sizeof(T) * (n))
#define     dyn_free(ptr)       dyn_free_typeless(ptr), ptr = NULL


typedef struct AllocNode_s AllocNode;

struct AllocNode_s
{
    i32         size;
    AllocNode*  prev;
    AllocNode*  next;
};
static AllocNode* MILTON_GLOBAL_dyn_freelist_sentinel;
static Arena* MILTON_GLOBAL_dyn_root_arena;

func void* dyn_alloc_typeless(i32 size)
{
    void* allocated = NULL;
    AllocNode* node = MILTON_GLOBAL_dyn_freelist_sentinel->next;
    while (node != MILTON_GLOBAL_dyn_freelist_sentinel)
    {
        if (node->size >= size)
        {
            // Remove node from list
            AllocNode* next = node->next;
            AllocNode* prev = node->prev;
            prev->next = next;
            next->prev = prev;

            allocated = (void*)((u8*)node + sizeof(AllocNode));

            // Found
            break;
        }
        node = node->next;
    }

    // If there was no viable candidate, get new pointer from root arena.
    if (!allocated)
    {
        void* mem = arena_alloc_bytes(MILTON_GLOBAL_dyn_root_arena, size + sizeof(AllocNode));
        if (mem)
        {
            AllocNode* node = (AllocNode*)mem;
            {
                node->size = size;
            }
            allocated = (void*)((u8*)mem + sizeof(AllocNode));
        }
        else
        {
            assert(!"Failed to do dynamic allocation.");
        }
    }

    return allocated;
}

func void dyn_free_typeless(void* dyn_ptr)
{
    // Insert at start of freelist.
    AllocNode* node = (AllocNode*)((u8*)dyn_ptr - sizeof(AllocNode));
    AllocNode* head = MILTON_GLOBAL_dyn_freelist_sentinel->next;
    head->prev->next = node;
    head->prev = node;

    node->next = head;

    node->prev = MILTON_GLOBAL_dyn_freelist_sentinel;

    memset(dyn_ptr, 0, node->size);  // Safety first!
}


// ==============================================
// ====   Simple stack from arena            ====
// ==== i.e. heap array with bounds checking ====
// ==============================================

/*
    This is a heap array with bounds checking.

    // Example: create an array of 10 elements of type Foo
    Foo* array = arena_make_stack(arena, 10, Foo);
    // Push into the array:
    stack_push(array, some_foo);
    // Get the first element:
    Foo* first = array[0];
    // Loop:
    for (int i = 0; i < stack_count(array); ++i)
    {
        Foo* foo = array[i];
        use_the_foo(foo);
    }
   */

#define arena_make_stack(a, size, type) (type *)arena__stack_typeless(a, sizeof(type) * size)
#define stack_push(a, e) if (arena__stack_check(a)) a[arena__stack_header(a)->count - 1] = e
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

func int arena__stack_check(void* stack)
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

