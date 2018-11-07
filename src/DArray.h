// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Dynamic array template class

#pragma once

#include "common.h"

#include "memory.h"
#include "platform.h"

template <typename T>
struct DArray
{
    i64     count;
    i64     capacity;
    T*      data;

    T&
    operator[](sz i)
    {
        mlt_assert(i < count);
        return data[i];
    }
};

template <typename T>
DArray<T>
dynamic_array(i64 capacity)
{
    DArray<T> arr;
    arr.count = 0;
    arr.capacity = capacity;
    arr.data = (T*)mlt_calloc(capacity, sizeof(T), "DArray");
    return arr;
}

template <typename T>
void
grow(DArray<T>* arr)
{
    // Default capacity.
    if ( arr->capacity == 0 ) {
        arr->capacity = 32;
    }
    while ( arr->capacity <= arr->count ) {
        arr->capacity *= 2;
    }
    if ( arr->data ) {
        arr->data = (T*)mlt_realloc(arr->data, (size_t)(arr->capacity*sizeof(T)), "DArray");
        if ( arr->data == NULL ) {
            milton_die_gracefully("Milton ran out of memory :(");
        }
    }
    else {
        arr->data = (T*)mlt_calloc((size_t)arr->capacity, sizeof(T), "DArray");
    }
}


template <typename T>
void
reserve(DArray<T>* arr, i64 size)
{
    if ( arr ) {
        if ( arr->capacity < size || arr->data == NULL ) {
            arr->capacity = size;
            grow(arr);
        }
    }
}

template <typename T>
T*
push(DArray<T>* arr, const T& elem)
{
    if ( arr->data == NULL ) {
        arr->capacity = 32;
        arr->count = 0;
        grow(arr);
    }
    else if ( arr->capacity <= arr->count ) {
        grow(arr);
    }
    arr->data[arr->count++] = elem;
    return &arr->data[arr->count-1];
}

template <typename T>
T*
get(DArray<T>* arr, i64 i)
{
    T* e = &arr->data[i];
    return e;
}

template <typename T>
T*
peek(DArray<T>* arr)
{
    T* elem = NULL;
    if ( arr->count > 0 ) {
        elem = &arr->data[arr->count-1];
    }
    return elem;
}

template <typename T>
T
pop(DArray<T>* arr)
{
    T elem = {};
    if ( arr->count > 0 ) {
        elem = arr->data[--arr->count];
    } else {
        mlt_assert(!"Attempting to pop from an empty array.");
    }
    return elem;
}

template <typename T>
i64
count(DArray<T>* arr)
{
    return arr->count;
}

template <typename T>
void
reset(DArray<T>* arr)
{
    arr->count = 0;
    // TODO: set to zero?
}

template <typename T>
void
release(DArray<T>* arr)
{
    if ( arr->data ) {
        mlt_free(arr->data, "DArray");
    }
}

// Iteration

template <typename T>
T*
begin(const DArray<T>& arr)
{
    T* result = NULL;
    if ( arr.count > 0 ) {
        result = &arr.data[0];
    }
    return result;
}

template <typename T>
T*
end(const DArray<T>& arr)
{
    T* result = NULL;
    if ( arr.count > 0 ) {
        result = arr.data + arr.count;
    }
    return result;
}

