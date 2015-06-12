#include "libserg/meta.h"

int main()
{
    meta_clear_file("vector.generated.h");
    meta_expand(
            "vector.generated.h",
            "vector.template.h",
            4,  // Number of bindings
            // Bindings:
            "Name2", "v2f",
            "Name3", "v3f",
            "Name4", "v4f",
            "type", "float");
    meta_expand(
            "vector.generated.h",
            "vector.template.h",
            4,  // Number of bindings
            // Bindings:
            "Name2", "v2i",
            "Name3", "v3i",
            "Name4", "v4i",
            "type", "int32");
    meta_expand(
            "vector.generated.h",
            "vector.template.h",
            4,  // Number of bindings
            // Bindings:
            "Name2", "v2l",
            "Name3", "v3l",
            "Name4", "v4l",
            "type", "int64");
}
