// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


#if MILTON_DEBUG


static void milton_cord_tests(Arena* arena);
static void milton_blend_tests();
static void milton_startup_tests();
static void milton_math_tests();

void milton_run_tests(MiltonState* milton_state)
{
    milton_math_tests();
    milton_blend_tests();
    milton_startup_tests();
}

static void milton_startup_tests()
{
    v3f rgb = hsv_to_rgb(v3f{ 0,0,0 });
    assert(rgb.r == 0 &&
           rgb.g == 0 &&
           rgb.b == 0);
    rgb = hsv_to_rgb(v3f{ 0, 0, 1.0 });
    assert(rgb.r == 1 &&
           rgb.g == 1 &&
           rgb.b == 1);
    rgb = hsv_to_rgb(v3f{ 120, 1.0f, 0.5f });
    assert(rgb.r == 0 &&
           rgb.g == 0.5f &&
           rgb.b == 0);
    rgb = hsv_to_rgb(v3f{ 0, 1.0f, 1.0f });
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

#else

// Avoid empty translation unit warning.
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(push, 0)

#endif  // _WIN32 && _MSC_VER

#endif  // MILTON_DEBUG
