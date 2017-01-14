#include "common.h"

u64
hash(char* string, size_t len)
{
    u64 h = 0;
    for ( size_t i = 0; i < len; ++i ) {
        h += string[i];
        h ^= h<<10;
        h += h>>5;
    }
    return h;
}