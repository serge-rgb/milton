export MILTON_OSX_FLAGS="-I../third_party/SDL2-2.0.3/include -I../third_party/imgui -I../third_party -framework OpenGL -framework AudioUnit -framework CoreAudio -framework Carbon -framework ForceFeedback -framework IOKit -framework Cocoa -liconv -lm"
export MILTON_SRC_DIR=`pwd`/src  # To get absolute paths in error msgs.


if [ ! -d build ]; then
    mkdir build
fi

cp third_party/Carlito.ttf build/
cp third_party/Carlito.LICENSE build/

cd build

clang++ -O0 -g \
   -Wno-c++11-compat-deprecated-writable-strings \
    -Wno-c++11-extensions \
    $MILTON_SRC_DIR/shadergen.cc -o shadergen

./shadergen


clang++ -O0 -g \
    -Isrc -Ithird_party -Ithird_party/imgui \
    -std=c++11 -Wall -Werror -Wno-missing-braces -Wno-unused-function \
    -Wno-unused-variable -Wno-unused-result -Wno-write-strings \
    -Wno-c++11-compat-deprecated-writable-strings -fno-strict-aliasing \
    -Wno-format-security \
    -Wno-format \
    -Wno-c++11-extensions \
    -fno-omit-frame-pointer \
    -Wno-unknown-pragmas \
    $MILTON_SRC_DIR/milton_unity_build.cc \
    ../third_party/build/libSDL2.a \
    ../third_party/build/libSDL2main.a \
    $MILTON_OSX_FLAGS \
    -o milton

cd ..


