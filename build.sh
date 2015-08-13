# Comment-out -Wno-unused-(variable|function) to clean up code
clang -Ithird_party \
    -std=c99\
    -Wall -Werror \
    -Wno-missing-braces \
    -Wno-unused-function \
    -Wno-unused-variable \
    `pkg-config --cflags sdl2` \
    -O2 -g\
    src/sdl_milton.c -lGL -lm -lpthread\
    `pkg-config --libs sdl2` \
    -o milton
