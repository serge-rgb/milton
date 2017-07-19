// Copyright (c) 2015-2017 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include "StrokeList.h"

void
strokelist_init_bucket(StrokeBucket* bucket)
{
    bucket->bounding_rect = rect_without_size();
}

static StrokeBucket*
create_bucket(Arena* arena)
{
    StrokeBucket* bucket = arena_alloc_elem(arena, StrokeBucket);
    strokelist_init_bucket(bucket);
    return bucket;
}

void
push(StrokeList* list, const Stroke& element)
{
    int bucket_i = list->count / STROKELIST_BUCKET_COUNT;
    int i = list->count % STROKELIST_BUCKET_COUNT;

    StrokeBucket* bucket = &list->root;
    while ( bucket_i != 0 ) {
        if ( !bucket->next ) {
            bucket->next = create_bucket(list->arena);
        }
        bucket = bucket->next;
        bucket_i -= 1;
    }

    bucket->data[i] = element;

    bucket->bounding_rect = rect_union(bucket->bounding_rect, element.bounding_rect);

    list->count += 1;
}

Stroke*
get(StrokeList* list, i64 idx)
{
    int bucket_i = idx / STROKELIST_BUCKET_COUNT;
    int i = idx % STROKELIST_BUCKET_COUNT;
    StrokeBucket* bucket = &list->root;
    while ( bucket_i != 0 ) {
        bucket = bucket->next;
        bucket_i -= 1;
    }
    return &bucket->data[i];
}

Stroke
pop(StrokeList* list)
{
    Stroke result = *get(list, list->count-1);
    list->count--;
    return result;
}

Stroke*
peek(StrokeList* list)
{
    Stroke* e = get(list, list->count-1);
    return e;
}

void
reset(StrokeList* list)
{
    list->count = 0;
    StrokeBucket* bucket = &list->root;

    while( bucket ) {
        bucket->bounding_rect = rect_without_size();
        bucket = bucket->next;
    }
}

i64
count(StrokeList* list)
{
    return list->count;
}

Stroke*
StrokeList::operator[] (i64 i)
{
    mlt_assert(i < this->count);
    Stroke* e = get(this, i);
    return e;
}
