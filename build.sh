cd src
clang metaprogram.c -o metaprogram
./metaprogram
cd ..

clang -Ithird_party -Ithird_party/SDL2-2.0.3/include \
    -lm -lpthread -ldl\
    -std=c99\
    src/sdl_milton.c -Lthird_party/build/ -lSDL2 -lGL -lm\
    -o milton
