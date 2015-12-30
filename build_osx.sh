export MILTON_OSX_FLAGS="-I../third_party/SDL2-2.0.3/include -I../third_party/imgui -I../third_party -framework OpenGL -framework AudioUnit -framework CoreAudio -framework Carbon -framework ForceFeedback -framework IOKit -framework Cocoa -liconv -lm"
export MILTON_SRC_DIR=`pwd`/src  # To get absolute paths in error msgs.


if [ ! -d build ]; then
    mkdir build
fi

cd build

# Omit -Wno-unused-(variable|function) to clean up code
clang++ -O0 -g -I../third_party $MILTON_SRC_DIR/headerlibs_impl.cc -c -o headerlibs_impl.o
ar rcs headerlibs_impl.a headerlibs_impl.o
clang++                 \
    -std=c++11          \
    -Wall -Werror       \
    -Wno-missing-braces \
    -Wno-unused-function \
    -Wno-unused-variable \
    -Wno-unused-result \
    -Wno-writable-strings \
    -Wno-c++11-compat-deprecated-writable-strings \
    -fno-strict-aliasing \
    -O0 -g                  \
    $MILTON_SRC_DIR/milton_unity_build.cc \
    headerlibs_impl.a        \
    ../third_party/build/libSDL2.a \
    ../third_party/build/libSDL2main.a \
    $MILTON_OSX_FLAGS \
    -o milton

cd ..

