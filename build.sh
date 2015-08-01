clang src/metaprogram.c -o src/metaprogram
cd src
./metaprogram
cd ..

# Comment-out -Wno-unused-variable to clean up code
clang -Ithird_party -Ithird_party/SDL2-2.0.3/include \
    -Wall -Werror \
    -Wno-missing-braces \
    -Wno-unused-function \
    -Wno-unused-variable \
    -lm -lpthread -ldl\
    -std=c99\
    -O2 -g\
    src/sdl_milton.c -Lthird_party/build/ -lSDL2 -lGL -lm\
    -o milton
