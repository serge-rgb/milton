// StrokeList
//
// - Works as a dynamically-sized array for Strokes.
// - Pointers to elements in the StrokeList stay valid for the lifetime of the program.

#define STROKELIST_BUCKET_COUNT 500

struct StrokeBucket
{
    Stroke          data[STROKELIST_BUCKET_COUNT];
    StrokeBucket*   next;
    Rect            bounding_rect;
};

struct StrokeList
{
    StrokeBucket    root;
    i64             count;
    Stroke*         operator[](i64 i);
};

StrokeBucket* create_bucket()
{
    StrokeBucket* bucket = (StrokeBucket*)mlt_calloc(1, sizeof(*bucket));
    bucket->bounding_rect = rect_without_size();
    return bucket;
}

void push(StrokeList* list, const Stroke& element)
{
    int bucket_i = list->count / STROKELIST_BUCKET_COUNT;
    int i = list->count % STROKELIST_BUCKET_COUNT;

    StrokeBucket* bucket = &list->root;
    while ( bucket_i != 0 ) {
        if ( !bucket->next ) {
            bucket->next = create_bucket();
        }
        bucket = bucket->next;
        bucket_i -= 1;
    }

    bucket->data[i] = element;

    bucket->bounding_rect = rect_union(bucket->bounding_rect, element.bounding_rect);

    list->count += 1;
}

Stroke* get(StrokeList* list, i64 idx)
{
    mlt_assert(idx < list->count);
    int bucket_i = idx / STROKELIST_BUCKET_COUNT;
    int i = idx % STROKELIST_BUCKET_COUNT;
    StrokeBucket* bucket = &list->root;
    while ( bucket_i != 0 ) {
        bucket = bucket->next;
        bucket_i -= 1;
    }
    return &bucket->data[i];
}

Stroke pop(StrokeList* list)
{
    Stroke result = *get(list, list->count-1);
    list->count--;
    return result;
}

Stroke* peek(StrokeList* list)
{
    Stroke* e = get(list, list->count-1);
    return e;
}

void reset(StrokeList* list)
{
    list->count = 0;
    StrokeBucket* bucket = &list->root;

    while( bucket ) {
        bucket->bounding_rect = rect_without_size();
        bucket = bucket->next;
    }
}

void release(StrokeList* list)
{
    list->count = 0;
    StrokeBucket* bucket = &list->root;
    while( bucket ) {
        StrokeBucket* next = bucket->next;
        if ( bucket != &list->root ) {
            mlt_free(bucket);
        }
        else {
            *bucket = {};
            bucket->bounding_rect = rect_without_size();
        }
        bucket = next;
    }
}

i64 count(StrokeList* list)
{
    return list->count;
}

Stroke* StrokeList::operator[] (i64 i)
{
    mlt_assert(i < this->count);
    Stroke* e = get(this, i);
    return e;
}
