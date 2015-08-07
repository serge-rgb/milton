clang src/metaprogram.c -o src/metaprogram
cd src
./metaprogram
cd ..

# Comment-out -Wno-unused-variable to clean up code
    #-lm -lpthread -ldl\
# Linux flags
#    `pkg-config --cflags sdl2` \
    # -lGL
    #-L ./third_party/build \

export MILTON_OSX_FLAGS="-I./third_party/SDL2-2.0.3/include -framework OpenGL -L/usr/local/lib -framework AudioUnit -framework CoreAudio -framework Carbon -framework ForceFeedback -framework IOKit -framework Cocoa -liconv -lm"

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
