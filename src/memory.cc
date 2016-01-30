// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.

#include "memory.h"


u8* arena_alloc_bytes(Arena* arena, size_t num_bytes)
{
    size_t total = arena->count + num_bytes;
    if (total > arena->size) {
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
    if (arena.ptr) {
        arena.size = size;
    }
    return arena;
}

Arena arena_spawn(Arena* parent, size_t size)
{
    u8* ptr = arena_alloc_bytes(parent, size);
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
    assert(parent);

    // Assert that this child was the latest push.
    assert ((parent->num_children - 1) == child->id);

    parent->count -= child->size;
    char* ptr = (char*)(parent->ptr) + parent->count;
    memset(ptr, 0, child->count);
    parent->num_children -= 1;

    //*child = Arena();
}

void arena_reset(Arena* arena)
{
    memset (arena->ptr, 0, arena->count);
    arena->count = 0;
}

