// ====
// Deque
// =====

// *Not* the textbook definition of a "double-ended queue"
// Using this name because of the performance characteristics that are similar the the C++ deque.
// Sorry if the name is confusing...
//
// It's a "stretchy" array without guarantee of locality.
// It's a linked list of chunks. Chunks are fixed-size arrays of length `chunk_size`

typedef struct $<T>DequeChunk_s  $<T>DequeChunk;
struct $<T>DequeChunk_s
{
    $<T>*	    data;
    $<T>DequeChunk* next;
};

typedef struct $<T>Deque_s
{
    Arena*	        parent_arena;
    $<T>DequeChunk*     first_chunk;
    i32	        chunk_size;
    i32	        count;
} $<T>Deque;

func $<T>DequeChunk* $<T>Deque_internal_alloc_chunk(Arena* arena, i32 chunk_size)
{
    $<T>DequeChunk* result = arena_alloc_elem(arena, $<T>DequeChunk);
    if (result)
    {
        result->data = arena_alloc_array(arena, chunk_size, $<T>);
        if (!result->data)
        {
            result = NULL;
        }
    }
    return result;
}

// Might return NULL, which is a failure case.
func $<T>Deque* $<T>Deque_make(Arena* arena, i32 chunk_size)
{
    $<T>Deque* deque = arena_alloc_elem(arena, $<T>Deque);
    if (deque)
    {
        deque->parent_arena = arena;
        deque->count = 0;
        deque->chunk_size = chunk_size;
        deque->first_chunk = $<T>Deque_internal_alloc_chunk(arena, chunk_size);
        if (!deque->first_chunk)
        {
            deque = NULL;
        }
    }
    return deque;
}

func void $<T>Deque_push($<T>Deque* deque, $<T> elem)
{
    $<T>DequeChunk* chunk = deque->first_chunk;
    int chunk_i = deque->count / deque->chunk_size;
    while(chunk_i--)
    {
        if (!chunk->next)
        {
            chunk->next = $<T>Deque_internal_alloc_chunk(deque->parent_arena, deque->chunk_size);
        }
        chunk = chunk->next;
        assert (chunk != NULL);
    }
    int elem_i = deque->count % deque->chunk_size;
    chunk->data[elem_i] = elem;
    deque->count += 1;
}

func $<T>* $<T>Deque_get($<T>Deque* deque, i32 i)
{
    $<T>DequeChunk* chunk = deque->first_chunk;
    int chunk_i = i / deque->chunk_size;
    while(chunk_i--)
    {
        chunk = chunk->next;
        assert (chunk != NULL);
    }
    int elem_i = i % deque->chunk_size;
    return &chunk->data[elem_i];
}

func $<T> $<T>Deque_pop($<T>Deque* deque, i32 i)
{
    $<T>DequeChunk* chunk = deque->first_chunk;
    int chunk_i = i / deque->chunk_size;
    while(chunk_i--)
    {
        chunk = chunk->next;
        assert (chunk != NULL);
    }
    int elem_i = i % deque->chunk_size;
    $<T> result = chunk->data[elem_i];
    deque->count -= 1;
    return result;
}
