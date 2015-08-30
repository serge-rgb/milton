#include "meta.h"

int main()
{
    // Vector
    meta_clear_file("vector.generated.h");
    meta_expand("vector.generated.h",
                "vector.template.h",
                4,  // Number of bindings
                // Bindings:
                "Name2", "v2f",
                "Name3", "v3f",
                "Name4", "v4f",
                "Type", "f32");
    meta_expand("vector.generated.h",
                "vector.template.h",
                4,  // Number of bindings
                // Bindings:
                "Name2", "v2i",
                "Name3", "v3i",
                "Name4", "v4i",
                "Type", "i32");
    meta_expand("vector.generated.h",
                "vector.template.h",
                4,  // Number of bindings
                // Bindings:
                "Name2", "v2l",
                "Name3", "v3l",
                "Name4", "v4l",
                "Type", "i64");


    // ==== StrokeDeque ====
    {
        meta_clear_file("StrokeDeque.generated.h");
        meta_expand("StrokeDeque.generated.h",
                    "deque.template.h",
                    1,
                    "T", "Stroke");
    }
}
