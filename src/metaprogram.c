#include "libserg/meta.h"

int main()
{
    meta_clear_file("vector.generated.h");
    meta_expand(
            "vector.generated.h",
            "vector.template.h",
            3,  // Number of bindings
            // Bindings:
            "name_2", "v2f",
            "name_3", "v3f",
            "type", "float");
    meta_expand(
            "vector.generated.h",
            "vector.template.h",
            3,  // Number of bindings
            // Bindings:
            "name_2", "v2i",
            "name_3", "v3i",
            "type", "int32");
    meta_expand(
            "vector.generated.h",
            "vector.template.h",
            3,  // Number of bindings
            // Bindings:
            "name_2", "v2l",
            "name_3", "v3l",
            "type", "int64");
}
