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

#pragma once

#define mlt_calloc(n, sz) calloc(n, sz)
#define mlt_free(ptr) do { if (ptr) { free(ptr); ptr = NULL; } else { assert(!"Freeing null"); } } while(0)
#define mlt_realloc realloc

struct Arena {
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


u8* arena_alloc_bytes(Arena* arena, size_t num_bytes);

