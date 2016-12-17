CC=clang
CXX=clang++

BUILDDIR    = build

SHELL = /bin/bash
CFLAGS = -O0 -g

INCLUDES = -I src \
           -I third_party \
           -I third_party/imgui \
#           -I third_party/SDL2-2.0.3/include

SHG_SOURCES      = src/shadergen.cc
MLT_SOURCES      = src/milton_unity_build.cc

SHG_CFLAGS = $(CFLAGS) \
			-std=c++11 \
			-Wno-writable-strings


MLT_CFLAGS = $(CFLAGS) \
             -std=c++11 \
             `pkg-config --cflags sdl2 gtk+-2.0` \
             -std=c++11 \
             -Wno-missing-braces \
             -Wno-unused-function \
             -Wno-unused-variable \
             -Wno-unused-result \
             -Wno-write-strings \
             -Wno-c++11-compat-deprecated-writable-strings \
			 -Wno-null-dereference \
			 -Wno-format \
             -fno-strict-aliasing \
             -fno-omit-frame-pointer \
             -ldl \
             -Werror

MLT_LDFLAGS = $(LDFLAGS) -lGL `pkg-config --libs sdl2 gtk+-2.0` -lXi

all: directories shadergen milton

shadergen:
	$(CC) $(SHG_CFLAGS) $(SHG_SOURCES) -o $(BUILDDIR)/shadergen

milton:
	pushd $(BUILDDIR) && ./shadergen && popd
	$(CXX) $(MLT_CFLAGS) $(INCLUDES) $(MLT_SOURCES) $(MLT_LDFLAGS) -o $(BUILDDIR)/milton

directories:
	@mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean directories
