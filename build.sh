#!/bin/bash


# Look for sdl
pkg-config --exists sdl2
sdl_ok=$?

if [ ! -d build ]; then
    echo "Building into new directory ./build"
    mkdir build
fi

cd build
if [ $sdl_ok -eq 0 ] && [ $? -eq 0 ]; then
    # Omit -Wno-unused-(variable|function) to clean up code
    clang++ -O2 -g -I../third_party ../src/headerlibs_impl.cc -c -o headerlibs_impl.o
    ar rcs headerlibs_impl.a headerlibs_impl.o
    clang++                 \
        -I../third_party       \
        -I../third_party/imgui \
	-std=c++11          \
	-Wall -Werror       \
	-Wno-missing-braces \
	-Wno-unused-function \
	-Wno-unused-variable \
	-Wno-unused-result \
        -Wno-c++11-compat-deprecated-writable-strings \
	-fno-strict-aliasing \
	`pkg-config --cflags sdl2` \
	-O2 -g                  \
	../src/milton_unity_build.cc -lGL -lm \
        headerlibs_impl.a        \
	`pkg-config --libs sdl2` \
	-lX11 -lXi \
	-o milton
else
    echo "SDL 2 not found."
    echo "   Please make sure that you have a development version of SDL2 installed. On"
    echo "   Ubuntu or other Debian-based distros, this is libsdl2-dev. \n\n"
    (exit 1);
fi

if [ $? -ne 0 ]; then
    echo "Milton build failed."
else
    echo "Milton build succeeded."
fi
cd ..
