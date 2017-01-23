export MILTON_OSX_FLAGS="-I../src -I../third_party/SDL2-2.0.3/include -I../third_party/imgui -I../third_party -framework OpenGL -framework AudioUnit -framework CoreAudio -framework Carbon -framework ForceFeedback -framework IOKit -framework Cocoa -liconv -lm"
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

pushd ../
./build/shadergen
popd


clang++ -O0 -g \
    -std=c++11 -Wall -Werror -Wno-missing-braces -Wno-unused-function \
    -Wno-unused-variable -Wno-unused-result -Wno-write-strings \
    -Wno-c++11-compat-deprecated-writable-strings -fno-strict-aliasing \
    -Wno-format-security \
    -Wno-format \
    -Wno-c++11-extensions \
    -fno-omit-frame-pointer \
    -Wno-unknown-pragmas \
    -Wno-extern-c-compat \
    $MILTON_SRC_DIR/milton.cc \
    $MILTON_SRC_DIR/memory.cc \
    $MILTON_SRC_DIR/gui.cc \
    $MILTON_SRC_DIR/persist.cc \
    $MILTON_SRC_DIR/color.cc \
    $MILTON_SRC_DIR/canvas.cc \
    $MILTON_SRC_DIR/profiler.cc \
    $MILTON_SRC_DIR/gl_helpers.cc \
    $MILTON_SRC_DIR/localization.cc \
    $MILTON_SRC_DIR/hardware_renderer.cc \
    $MILTON_SRC_DIR/utils.cc \
    $MILTON_SRC_DIR/hash.cc \
    $MILTON_SRC_DIR/vector.cc \
    $MILTON_SRC_DIR/sdl_milton.cc \
    $MILTON_SRC_DIR/StrokeList.cc \
    $MILTON_SRC_DIR/platform_mac.cc \
    $MILTON_SRC_DIR/platform_unix.cc \
    $MILTON_SRC_DIR/third_party_libs.cc \
    ../third_party/build/libSDL2.a \
    ../third_party/build/libSDL2main.a \
    $MILTON_OSX_FLAGS \
    -o milton

cd ..


