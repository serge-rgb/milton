#include "meta.h"
#include "memory.c"

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


    // ==== StrokeCord ====
    {
        char* sources[2] = {
            "cord.template.h",
            "cord.template.c",
        };
        char* dests[2] = {
            "StrokeCord.generated.h",
            "StrokeCord.generated.c",
        };
        for ( int i = 0; i < 2; ++i )
        {
            meta_clear_file(dests[i]);
            meta_expand(dests[i],
                        sources[i],
                        1,
                        "T", "Stroke");
        }
    }
}
