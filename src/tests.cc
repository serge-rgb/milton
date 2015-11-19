
#include "tests.h"

#include "memory.h"
#include "Array.h"

static void milton_dynmem_tests();
static void milton_cord_tests(Arena* arena);
static void milton_blend_tests();
static void milton_startup_tests();
static void milton_math_tests();
static void array_tests();

void milton_run_tests(MiltonState* milton_state)
{
    array_tests();
    milton_math_tests();
    milton_dynmem_tests();
    milton_cord_tests(milton_state->root_arena);
    milton_blend_tests();
    milton_startup_tests();
}

#ifndef NDEBUG

// Inspect with debugger to make sure that RAII stuff is working.
static void array_tests()
{
    ScopedArray<u8> out_scoped;
    {
        ScopedArray<u8> scoped(10,(u8*)mlt_calloc(10, 1) );
        u8 i=0;
        for (auto& e : scoped) {
            e = i++;
        }
        out_scoped = scoped;  // Ownership transfer.
    }
    {
        u8 i = 0;
        for (auto e: out_scoped) {
            assert(e == i++);
        }
    }

    StretchArray<i32> stretch(1);
    {
        StretchArray<i32> in_stretch(1);
        for (i32 i = 0; i < 5; ++i)
            push(in_stretch, i);
        // Test ownership
        stretch = in_stretch;
    }
    {
        i32 i = 0;
        for (auto e : stretch) {
            assert(e == i++);
        }
    }
}

static void milton_startup_tests()
{
    v3f rgb = hsv_to_rgb({ 0,0,0 });
    assert(rgb.r == 0 &&
           rgb.g == 0 &&
           rgb.b == 0);
    rgb = hsv_to_rgb({ 0, 0, 1.0 });
    assert(rgb.r == 1 &&
           rgb.g == 1 &&
           rgb.b == 1);
    rgb = hsv_to_rgb({ 120, 1.0f, 0.5f });
    assert(rgb.r == 0 &&
           rgb.g == 0.5f &&
           rgb.b == 0);
    rgb = hsv_to_rgb({ 0, 1.0f, 1.0f });
    assert(rgb.r == 1.0f &&
           rgb.g == 0 &&
           rgb.b == 0);
}

static void milton_blend_tests()
{
    v4f a = { 1,0,0, 0.5f };
    v4f b = { 0,1,0, 0.5f };
    v4f blend = blend_v4f(a, b);
    assert (blend.r > 0);
}

static void milton_math_tests()
{
    v2i a = { 0,  0 };
    v2i b = { 2,  0 };
    v2i u = { 1, -2 };
    v2i v = { 1,  2 };

    v2f intersection;
    b32 hit = intersect_line_segments(a, b,
                                      u, v,
                                      &intersection);
    assert(hit);
    assert(intersection.y == 0);
    assert(intersection.x >= 0.99999 && intersection.x <= 1.00001f);
}

static void milton_cord_tests(Arena* arena)
{
    StrokeCord* cord = StrokeCord_make(arena, 2);
    if ( cord ) {
        for (int i = 0; i < 11; ++i) {
            Stroke test = {0};
            test.num_points = i;
            push(cord, test);
        }
        for ( int i = 0; i < 11; ++i ) {
            Stroke test = *get(cord, i);
            assert(test.num_points == i);
        }
    } else {
        assert (!"Could not create cord");
    }
}

static void milton_dynmem_tests()
{
    int* ar1 = dyn_alloc(int, 10);
    int* ar0 = dyn_alloc(int, 1);

    dyn_free(ar1);
    dyn_free(ar0);

    int* ar2 = dyn_alloc(int, 9);
    dyn_free(ar2);
}

#endif
