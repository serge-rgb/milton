// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// StrokeList
//
// - Works as a dynamically-sized array for Strokes.
// - Pointers to elements in the StrokeList stay valid for the lifetime of the program.


#pragma once

#include "stroke.h"

#include "memory.h"

#define STROKELIST_BUCKET_COUNT 4196

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

    Arena*          arena;
};

void strokelist_init_bucket(StrokeBucket* bucket);

void push(StrokeList* list, const Stroke& element);
Stroke* get(StrokeList* list, i64 idx);
Stroke pop(StrokeList* list);
Stroke* peek(StrokeList* list);
void reset(StrokeList* list);
i64 count(StrokeList* list);

struct StrokeIterator;

Stroke* stroke_iter_init(StrokeList* list, StrokeIterator* iter);
Stroke* stroke_iter_next(StrokeIterator* iter);