cd src
clang template_expand.c -g -o template_expand
./template_expand
cd ..

# Comment-out -Wno-unused-(variable|function) to clean up code
clang -Ithird_party \
    -std=c99\
    -Wall -Werror \
    -Wno-missing-braces \
    -Wno-unused-function \
    -Wno-unused-variable \
    `pkg-config --cflags sdl2` \
    -Ithird_party/gui \
    -O2 -g\
    src/sdl_milton.c -lGL -lm \
    `pkg-config --libs sdl2` \
    -lX11 -lXi \
    -o milton
