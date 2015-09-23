// ====
// Cord
// =====

// A "stretchy" array that works well with arena allocators.
// It's a linked list of chunks. Chunks are fixed-size arrays of length `chunk_size`

typedef struct $<T>CordChunk_s  $<T>CordChunk;
struct $<T>CordChunk_s {
    $<T>*	   data;
    $<T>CordChunk* next;
};

typedef struct $<T>Cord_s {
    Arena*	        parent_arena;
    $<T>CordChunk*      first_chunk;
    i32	        chunk_size;
    i32	        count;
} $<T>Cord;

func $<T>CordChunk* $<T>Cord_internal_alloc_chunk(Arena* arena, i32 chunk_size) {
    $<T>CordChunk* result = arena_alloc_elem(arena, $<T>CordChunk);
    if (result) {
        result->data = arena_alloc_array(arena, chunk_size, $<T>);
        if (!result->data) {
            result = NULL;
        }
    }
    return result;
}

// Might return NULL, which is a failure case.
func $<T>Cord* $<T>Cord_make(Arena* arena, i32 chunk_size) {
    $<T>Cord* deque = arena_alloc_elem(arena, $<T>Cord);
    if (deque) {
        deque->parent_arena = arena;
        deque->count = 0;
        deque->chunk_size = chunk_size;
        deque->first_chunk = $<T>Cord_internal_alloc_chunk(arena, chunk_size);
        if (!deque->first_chunk) {
            deque = NULL;
        }
    }
    return deque;
}

func b32 $<T>Cord_push($<T>Cord* deque, $<T> elem)
{
    b32 succeeded = false;
    $<T>CordChunk* chunk = deque->first_chunk;
    int chunk_i = deque->count / deque->chunk_size;
    while(chunk_i--) {
        if (!chunk->next) {
            chunk->next = $<T>Cord_internal_alloc_chunk(deque->parent_arena, deque->chunk_size);
        }
        chunk = chunk->next;
    }
    if (chunk) {
        int elem_i = deque->count % deque->chunk_size;
        chunk->data[elem_i] = elem;
        deque->count += 1;
        succeeded = true;
    }
    return succeeded;
}

func $<T>* $<T>Cord_get($<T>Cord* deque, i32 i)
{
    $<T>CordChunk* chunk = deque->first_chunk;
    int chunk_i = i / deque->chunk_size;
    while(chunk_i--) {
        chunk = chunk->next;
        assert (chunk != NULL);
    }
    int elem_i = i % deque->chunk_size;
    return &chunk->data[elem_i];
}

func $<T> $<T>Cord_pop($<T>Cord* deque, i32 i)
{
    $<T>CordChunk* chunk = deque->first_chunk;
    int chunk_i = i / deque->chunk_size;
    while(chunk_i--) {
        chunk = chunk->next;
        assert (chunk != NULL);
    }
    int elem_i = i % deque->chunk_size;
    $<T> result = chunk->data[elem_i];
    deque->count -= 1;
    return result;
}

