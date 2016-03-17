// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.



#if MILTON_DEBUG

#include "tests.h"

#include "memory.h"

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
    v3f rgb = hsv_to_rgb((v3f){ 0,0,0 });
    assert(rgb.r == 0 &&
           rgb.g == 0 &&
           rgb.b == 0);
    rgb = hsv_to_rgb((v3f){ 0, 0, 1.0 });
    assert(rgb.r == 1 &&
           rgb.g == 1 &&
           rgb.b == 1);
    rgb = hsv_to_rgb((v3f){ 120, 1.0f, 0.5f });
    assert(rgb.r == 0 &&
           rgb.g == 0.5f &&
           rgb.b == 0);
    rgb = hsv_to_rgb((v3f){ 0, 1.0f, 1.0f });
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
