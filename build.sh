clang src/metaprogram.c -o src/metaprogram
cd src
./metaprogram
cd ..

# Comment-out -Wno-unused-variable to clean up code
    #-lm -lpthread -ldl\
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
