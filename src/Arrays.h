//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <memory>

#include "common.h"

// Dumb array
template <typename T> struct Array
{
    INSERT_ALLOC_OVERRIDES

    T*      m_data;
    size_t  m_count;

    T& operator[] (const size_t i)
    {
        assert (i < m_count);
        return m_data[i];
    }
};

template <typename T> size_t count(Array<T>& arr)
{
    return arr.m_count;
}

// Iterator support (range-based for).
template<class T> T* begin(Array<T> a) { return a.m_data; }
template<class T> T* end(Array<T> a) { return a.m_data + a.m_count; }


template<typename T> struct ScopedArray
{
    INSERT_ALLOC_OVERRIDES

    T*                  m_data;
    size_t              m_count;

    ScopedArray()
    {
        m_data = NULL;
        m_count = 0;
        // Would be nice to zero-out members automatically.
    }

    ScopedArray(size_t count, T* data)
    {
        m_data = data;
        m_count = count;
    }

    ScopedArray(size_t count)
    {
        T* data = (T*)mlt_calloc(count, sizeof(T));
        if (data) {
            m_data = data;
            m_count = count;
        } else {
            milton_die_gracefully("Out of memory error allocating array.\n");
        }
    }

    virtual ~ScopedArray()
    {
        if (m_data) {
            mlt_free(m_data);
        }
    }

    T& operator[](const size_t i)
    {
        assert(i < m_count);
        return m_data[i];
    }


    // Shallow copy. Ownership transfer.
    ScopedArray<T>& operator=(ScopedArray<T>& other)
    {
        if (m_data) {
            mlt_free(m_data);
        }

        m_data  = other.m_data;
        m_count = other.m_count;

        other.m_data  = NULL;
        other.m_count = 0;
        return *this;
    }
};

// Nice for containers to conform to certain interfaces.
template<class T> size_t count(ScopedArray<T>& arr)
{
    return arr.m_count;
}
// Iterator support (range-based for).
template<class T> T* begin(ScopedArray<T>& a) { return a.m_data; }
template<class T> T* end(ScopedArray<T>& a) { return a.m_data + a.m_count; }

template<class T> Array<T> get_dumb_array(ScopedArray<T>& scoped_arr)
{
    Array<T> arr;
    arr.m_data  = scoped_arr.m_data;
    arr.m_count = scoped_arr.m_count;
    return arr;
}


template<class T> struct StretchArray
{
    INSERT_ALLOC_OVERRIDES

    T*      m_data;
    size_t  m_count;
    size_t  m_capacity;

    StretchArray() : m_data(0), m_count(0), m_capacity(0) {}
    StretchArray(size_t capacity)
    {
        m_data     = nullptr;
        m_count    = 0;
        m_capacity = 0;

        T* data = (T*)mlt_calloc(capacity, sizeof(T));
        if (data) {
            m_capacity = capacity;
            m_data     = data;
        } else {
            milton_die_gracefully("Out of memory in StretchArray construction\n");
        }
    }
    virtual ~StretchArray()
    {
        if (m_data)
            mlt_free(m_data);
    }

    T& operator[](const size_t i)
    {
        assert(i < m_count);
        return m_data[i];
    }

    // Shallow copy. Ownership transfer.
    StretchArray<T>& operator=(StretchArray<T>& other)
    {
        if (m_data) {
            mlt_free(m_data);
        }
        m_data     = other.m_data;
        m_count    = other.m_count;
        m_capacity = other.m_capacity;

        other.m_data     = nullptr;
        other.m_count    = 0;
        other.m_capacity = 0;
        return *this;
    }
};

template<class T> void push(StretchArray<T>& arr, const T& e)
{
    if (arr.m_count >= arr.m_capacity) {
        arr.m_capacity *= 2;
        T* oldptr = arr.m_data;
        arr.m_data = (T*)mlt_realloc(arr.m_data, sizeof(T)*arr.m_capacity);
        if (!arr.m_data) {
            mlt_free(oldptr);  // realloc doesn't free old pointer on fail.
            milton_die_gracefully("Out of memory pushing to StretchArray.\n");
        }
    }

    arr[arr.m_count++] = e;
}

template<class T> size_t count(StretchArray<T>& arr) { return arr.m_count; }
// Iterator support (range-based for).
template<class T> T* begin(StretchArray<T>& a) { return a.m_data; }
template<class T> T* end(StretchArray<T>& a) { return a.m_data + a.m_count; }
