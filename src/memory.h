// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#pragma once

#include "common.h"

// TODO: out of memory handler.

#define mlt_malloc(sz) INVALID_CODE_PATH
#if DEBUG_MEMORY_USAGE
    #define mlt_calloc(n, sz, category) calloc_with_debug(n, sz, category, __FILE__, __LINE__)
    #define mlt_free(ptr, category) free_with_debug(ptr, category); ptr=NULL
    #define mlt_realloc(ptr, sz, category) realloc_with_debug(ptr, sz, category, __FILE__, __LINE__)
#else
    #define mlt_calloc(n, sz, category) calloc(n, sz)
    #define mlt_free(ptr, category) do { if (ptr) { free(ptr); ptr = NULL; } else { mlt_assert(!"Freeing null"); } } while(0)
    #define mlt_realloc(ptr, sz, category) realloc(ptr, sz)
#endif


struct Arena
{
    // Memory:
    size_t  size;
    size_t  count;
    size_t  min_block_size;
    u8*     ptr;

    // For pushing/popping
    Arena*  parent;
    int     id;
    int     num_children;
};

// Stored at the end of the arena.
// If the arena expands, its memory block will point to previous memory blocks.
struct ArenaFooter
{
    u8*     previous_block;
    size_t  previous_size;
};

// Create a root arena from a memory block.
Arena arena_init(size_t min_block_size = 0, void* base = NULL);
Arena arena_spawn(Arena* parent, size_t size);
void  arena_reset(Arena* arena);
void  arena_reset_noclear(Arena* arena);
void  arena_free(Arena* arena);

// ==== Temporary arenas.
// Usage:
//      child = arena_push(my_arena, some_size);
//      use_temporary_arena(&child.arena);
//      arena_pop(child);
Arena  arena_push(Arena* parent, size_t size = 0);
void   arena_pop(Arena* child);
void   arena_pop_noclear(Arena* child);

#define     arena_alloc_elem_(arena, T, flags)          (T *)arena_alloc_bytes((arena), sizeof(T), flags)
#define     arena_alloc_array_(arena, count, T, flags)  (T *)arena_alloc_bytes((arena), (count) * sizeof(T), flags)
#define     arena_alloc_elem(arena, T)                  arena_alloc_elem_(arena, T, Arena_NONE)
#define     arena_alloc_array(arena, count, T)          arena_alloc_array_(arena, count, T, Arena_NONE)
#define     ARENA_VALIDATE(arena)                       mlt_assert ((arena)->num_children == 0)
#define     arena_bootstrap(Type, member, size)         (Type*)arena_bootstrap_(size, sizeof(Type), offsetof(Type, member))

enum ArenaAllocOpts
{
    Arena_NONE = 0,

    Arena_NOFAIL = 1<<0,
};

u8* arena_alloc_bytes(Arena* arena, size_t num_bytes, int alloc_flags=Arena_NONE);

void* arena_bootstrap_(size_t size, size_t obj_size, size_t offset);

void* calloc_with_debug(size_t n, size_t sz, char* category, char* file, i64 line);
void  free_with_debug(void* ptr, char* category);
void* realloc_with_debug(void* ptr, size_t sz, char* category, char* file, i64 line);
void debug_memory_dump_allocations();
