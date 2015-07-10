clang -Ithird_party -Ithird_party/SDL2-2.0.3/include \
    -lm -lpthread -ldl\
    src/sdl_milton.c -Lthird_party/build/ -lSDL2 -lSDL2 \
    -o milton
