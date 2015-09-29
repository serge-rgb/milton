#pragma once

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

$<T>CordChunk* $<T>Cord_internal_alloc_chunk(Arena* arena, i32 chunk_size);

// Might return NULL, which is a failure case.
$<T>Cord* $<T>Cord_make(Arena* arena, i32 chunk_size);

b32 $<T>Cord_push($<T>Cord* deque, $<T> elem);

$<T>* $<T>Cord_get($<T>Cord* deque, i32 i);

$<T> $<T>Cord_pop($<T>Cord* deque, i32 i);

