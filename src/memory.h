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

struct Arena_s {
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
Arena arena_init(void* base, size_t size);
Arena arena_spawn(Arena* parent, size_t size);
void  arena_reset(Arena* arena);

// ==== Temporary arenas.
// Usage:
//      child = arena_push(my_arena, some_size);
//      use_temporary_arena(&child.arena);
//      arena_pop(child);
Arena  arena_push(Arena* parent, size_t size);
void   arena_pop (Arena* child);

#define     arena_alloc_elem(arena, T)          (T *)arena_alloc_bytes((arena), sizeof(T))
#define     arena_alloc_array(arena, count, T)  (T *)arena_alloc_bytes((arena), (count) * sizeof(T))
#define     arena_available_space(arena)        ((arena)->size - (arena)->count)
#define     ARENA_VALIDATE(arena)               assert ((arena)->num_children == 0)


void* arena_alloc_bytes(Arena* arena, size_t num_bytes);

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

void* dyn_alloc_typeless(i32 size);
void dyn_free_typeless(void* dyn_ptr);

typedef struct AllocNode_s AllocNode;

struct AllocNode_s {
    i32         size;
    AllocNode*  prev;
    AllocNode*  next;
};
extern AllocNode* MILTON_GLOBAL_dyn_freelist_sentinel;
extern Arena* MILTON_GLOBAL_dyn_root_arena;
