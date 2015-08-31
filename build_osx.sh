
cd src
clang template_expand.c -g -o template_expand
./template_expand
cd ..

export MILTON_OSX_FLAGS="-I./third_party/SDL2-2.0.3/include -I./third_party/gui -framework OpenGL -L/usr/local/lib -framework AudioUnit -framework CoreAudio -framework Carbon -framework ForceFeedback -framework IOKit -framework Cocoa -liconv -lm"

# Comment-out -Wno-unused-variable to clean up code
clang -Ithird_party \
    -std=c99\
    -Wall -Werror \
    -Wno-missing-braces \
    -Wno-unused-function \
    -Wno-unused-variable \
    -O2 -g\
    $MILTON_OSX_FLAGS \
    src/sdl_milton.c -lm -lpthread\
    third_party/build/libSDL2.a \
    third_party/build/libSDL2main.a \
    -o milton
