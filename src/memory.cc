// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


u8*
arena_alloc_bytes(Arena* arena, size_t num_bytes, int alloc_flags)
{
    size_t total = arena->count + num_bytes;
    if ( total > arena->size ) {
        size_t new_size = MAX(num_bytes, arena->min_block_size);
        ArenaFooter arena_footer = {};
        arena_footer.previous_block = arena->ptr;
        arena_footer.previous_size = arena->size;
        arena->ptr = (u8*)platform_allocate(new_size + sizeof(ArenaFooter));
        arena->size = new_size;
        arena->count = 0;
        *(ArenaFooter*)(arena->ptr + arena->size) = arena_footer;
    }
    u8* result = arena->ptr + arena->count;
    arena->count += num_bytes;
    return result;
}

Arena
arena_init(size_t min_block_size, void* base)
{
    Arena arena = {};
    if ( min_block_size ) {
        arena.min_block_size = min_block_size;
    }
    else {
        arena.min_block_size = 1024;
    }
    if ( base ) {
        arena.ptr = (u8*)base;
    }
    else {
        arena.ptr = (u8*)platform_allocate(arena.min_block_size + sizeof(ArenaFooter));
    }

    if ( arena.ptr ) {
        arena.size = arena.min_block_size;
    }
    else {
        milton_die_gracefully("Could not allocate memory for arena.");
    }

    ArenaFooter footer = {};
    *(ArenaFooter*)(arena.ptr + sizeof(ArenaFooter)) = footer;
    return arena;
}

void*
arena_bootstrap_(size_t size, size_t obj_size, size_t offset)
{
    Arena arena = arena_init(size + obj_size);
    *(Arena*)(arena.ptr + offset) = arena;
    return arena_alloc_bytes((Arena*)(arena.ptr + offset), obj_size);
}


void
arena_free(Arena* arena)
{
    while ( arena->ptr ) {
        ArenaFooter footer = *(ArenaFooter*)(arena->ptr + arena->size);
        platform_deallocate(arena->ptr);
        arena->ptr = footer.previous_block;
        arena->size = footer.previous_size;
    }
}

Arena
arena_spawn(Arena* parent, size_t size)
{
    u8* ptr = arena_alloc_bytes(parent, size + sizeof(ArenaFooter));
    mlt_assert(ptr);

    Arena child = { 0 };
    {
        child.ptr    = ptr;
        child.size   = size;
    }

    return child;
}

Arena
arena_push(Arena* parent, size_t in_size)
{
    size_t size;
    if ( in_size ) {
        size = in_size;
    } else {
        size = parent->min_block_size;
    }
    Arena child = { 0 };
    {
        child.parent = parent;
        child.id     = parent->num_children;
        u8* ptr = arena_alloc_bytes(parent, size + sizeof(ArenaFooter));
        parent->num_children += 1;
        child.ptr = ptr;
        child.size = size;
    }
    return child;
}

void
arena_pop(Arena* child)
{
    Arena* parent = child->parent;
    mlt_assert(parent);

    // Assert that this child was the latest push.
    mlt_assert ((parent->num_children - 1) == child->id);
    ArenaFooter* footer = (ArenaFooter*)(child->ptr + child->size);
    while ( footer->previous_block ) {
        platform_deallocate(child->ptr);
        child->size = footer->previous_size;
        child->ptr = footer->previous_block;
        footer = (ArenaFooter*)(child->ptr + child->size);
    }
    parent->count -= child->size;
    char* ptr = (char*)(parent->ptr) + parent->count;
    memset(ptr, 0, child->count);
    parent->num_children -= 1;
}

void
arena_pop_noclear(Arena* child)
{
    Arena* parent = child->parent;
    mlt_assert(parent);

    // Assert that this child was the latest push.
    mlt_assert ((parent->num_children - 1) == child->id);

    parent->count -= child->size;
    parent->num_children -= 1;
}

void
arena_reset(Arena* arena)
{
    memset (arena->ptr, 0, arena->count);
    arena->count = 0;
}

void
arena_reset_noclear(Arena* arena)
{
    arena->count = 0;
}

#if DEBUG_MEMORY_USAGE

#define NUM_MEMORY_DEBUG_BUCKETS    32
#define HASH_TABLE_SIZE             1023

struct AllocationDebugInfo
{
    // Number of allocation in buckets.
    //  allocation_counts[i][_] := # of allocations less than or equal to 2^i bytes
    size_t allocation_counts[NUM_MEMORY_DEBUG_BUCKETS][HASH_TABLE_SIZE];
    size_t allocation_bytes[NUM_MEMORY_DEBUG_BUCKETS][HASH_TABLE_SIZE];
};

static AllocationDebugInfo g_allocation_debug_info;

static char* g_allocation_categories[HASH_TABLE_SIZE];


static u64
find_bucket_for_size(size_t sz)
{
    u64 bucket = NUM_MEMORY_DEBUG_BUCKETS;
    // There is probably a bit twiddling hack to compute this less stupidly
    for ( size_t b = 0; b < NUM_MEMORY_DEBUG_BUCKETS; ++b ) {
        if ( sz <= 1<<b ) {
            bucket = b;
            break;
        }
    }
    if ( bucket == NUM_MEMORY_DEBUG_BUCKETS ) {
        milton_die_gracefully("Allocation too large.");
    }
    return bucket;
}

void*
calloc_with_debug(size_t n, size_t sz, char* category)
{
    if ( strcmp(category, "Layer") == 0 ) {
        int breakhere=1;
    }
    u64 h = hash(category, strlen(category));

    // Try and store the category if we haven't seen it before.
    u64 index = h%HASH_TABLE_SIZE;

    if ( g_allocation_categories[index] && strcmp(g_allocation_categories[index], category) != 0 ) {
        milton_die_gracefully("We can't have collisions.\n");
    }

    if ( index != HASH_TABLE_SIZE+1 ) {
        g_allocation_categories[index] = category;
    }

    u64 b = find_bucket_for_size(n*sz);

    milton_log("Received allocation with %d bytes in category %s\n", n*sz, category);

    // Register this allocation
    milton_log("Storing in bucket %d with hash %d\n", b, h%HASH_TABLE_SIZE);
    g_allocation_debug_info.allocation_counts[b][index] += 1;
    g_allocation_debug_info.allocation_bytes[b][index] += n*sz;

    return calloc(n, sz);
}

void
free_with_debug(void* ptr, char* category)
{
    free(ptr);
}

void*
realloc_with_debug(void* ptr, size_t sz, char* category)
{
    return realloc(ptr, sz);
}

void
debug_memory_dump_allocations()
{
    for ( size_t i = 0; i < HASH_TABLE_SIZE; ++i ) {
        if ( g_allocation_categories[i] ) {
            milton_log("For category %s...\n", g_allocation_categories[i]);
            for ( size_t b = 0; b < NUM_MEMORY_DEBUG_BUCKETS; ++b ) {
                size_t num_allocs = g_allocation_debug_info.allocation_counts[b][i];
                milton_log("    Number of allocations of %d bytes is %d\n", 1<<b, num_allocs);
            }
        }
    }

}

#endif  // DEBUG_MEMORY_USAGE