// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
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
    mlt_assert(list->count > 0);
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

struct StrokeIterator
{
    StrokeBucket* cur_bucket;
    int i;
    int count;
};

Stroke* stroke_iter_init_at(StrokeList* list, StrokeIterator* iter, u64 stroke_i)
{
    Stroke* result = NULL;

    iter->cur_bucket = &list->root;
    iter->count = list->count;
    iter->i = 0;

    if (stroke_i < iter->count) {
        iter->i = stroke_i;
        for (u64 i = 0; i < stroke_i / STROKELIST_BUCKET_COUNT; ++i) {
            iter->cur_bucket = iter->cur_bucket->next;
        }

        result = &iter->cur_bucket->data[iter->i % STROKELIST_BUCKET_COUNT];
    }


    return result;
}

Stroke* stroke_iter_init(StrokeList* list, StrokeIterator* iter)
{
    Stroke* result = stroke_iter_init_at(list, iter, 0);
    return result;
}

Stroke* stroke_iter_next(StrokeIterator* iter)
{
    Stroke* result = NULL;

    if (iter->cur_bucket) {
        iter->i++;
        if ((iter->i % STROKELIST_BUCKET_COUNT) == 0) {
            iter->cur_bucket = iter->cur_bucket->next;
        }

        if (iter->i < iter->count) {
            int bucket_i = iter->i % STROKELIST_BUCKET_COUNT;
            result = &iter->cur_bucket->data[bucket_i];
        }
    }

    return result;
}
