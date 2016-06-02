// Copyright (c) 2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Dynamic array template class.


#include "common.h"

template <typename T>
struct DArray
{
    u64     count;
    u64     capacity;
    T*      data;
};

template <typename T>
DArray<T> dynamic_array(u64 capacity)
{
    DArray<T> arr;
    arr.count = 0;
    arr.capacity = capacity;
    arr.data = (T*)mlt_calloc(capacity, sizeof(T));
    return arr;
}

template <typename T>
void grow(DArray<T>* arr)
{
    if (arr->capacity == 0)
    {
        arr->capacity = 32;
    }
    while (arr->capacity <= arr->count)
    {
        arr->capacity *= 2;
    }
    if (arr->data)
    {
        arr->data = (T*)mlt_realloc(arr->data, arr->capacity*sizeof(T));
        if (arr->data == NULL)
        {
            milton_die_gracefully("Milton ran out of memory :(");
        }
    }
    else
    {
        arr->data = (T*)mlt_calloc(arr->capacity, sizeof(T));
    }
}


template <typename T>
void reserve(DArray<T>* arr, u64 size)
{
    if (arr)
    {
        arr->capacity = size;
        if (arr->capacity < size || arr->data == NULL)
        {
            grow(arr);
        }
    }
}

template <typename T>
void push(DArray<T>* arr, const T& elem)
{
    if (arr->data == NULL)
    {
        arr->capacity = 32;
        arr->count = 0;
        grow(arr);
    }
    else if (arr->capacity <= arr->count)
    {
        grow(arr);
    }
    arr->data[arr->count++] = elem;
}

template <typename T>
T* peek(DArray<T>* arr)
{
    T* elem = NULL;
    if (arr->count > 0)
    {
        elem = &arr->data[arr->count-1];
    }
    return elem;
}

template <typename T>
T pop(DArray<T>* arr)
{
    T elem = {};
    if (arr->count > 0)
    {
        elem = arr->data[--arr->count];
    }
    return elem;
}

template <typename T>
void reset(DArray<T>* arr)
{
    arr->count = 0;
    // TODO: set to zero?
}

template <typename T>
void release(DArray<T>* arr)
{
    if (arr->data)
    {
        mlt_free(arr->data);
    }
}
