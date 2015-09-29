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


AllocNode* MILTON_GLOBAL_dyn_freelist_sentinel = NULL;
Arena* MILTON_GLOBAL_dyn_root_arena = NULL;

void* dyn_alloc_typeless(i32 size)
{
    void* allocated = NULL;
    AllocNode* node = MILTON_GLOBAL_dyn_freelist_sentinel->next;
    while (node != MILTON_GLOBAL_dyn_freelist_sentinel) {
        if (node->size >= size) {
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
    if (!allocated) {
        void* mem = arena_alloc_bytes(MILTON_GLOBAL_dyn_root_arena, size + sizeof(AllocNode));
        if (mem) {
            node = (AllocNode*)mem;
            node->size = size;
            allocated = (void*)((u8*)mem + sizeof(AllocNode));
        } else {
            assert(!"Failed to do dynamic allocation.");
        }
    }

    return allocated;
}

void dyn_free_typeless(void* dyn_ptr)
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

void* arena_alloc_bytes(Arena* arena, size_t num_bytes)
{
    size_t total = arena->count + num_bytes;
    if (total > arena->size) {
        return NULL;
    }
    void* result = arena->ptr + arena->count;
    arena->count += num_bytes;
    return result;
}

Arena arena_init(void* base, size_t size)
{
    Arena arena = { 0 };
    arena.ptr = (u8*)base;
    if (arena.ptr) {
        arena.size = size;
    }
    return arena;
}

Arena arena_spawn(Arena* parent, size_t size)
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

Arena arena_push(Arena* parent, size_t size)
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

void arena_pop(Arena* child)
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

void arena_reset(Arena* arena)
{
    memset (arena->ptr, 0, arena->count);
    arena->count = 0;
}

