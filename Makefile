CC=clang++
LD=clang++

#CFLAGS = -O0 -g
CFLAGS = -O3 -g

LDFLAGS = -lm

MLT_CFLAGS = $(CFLAGS) \
	     -Isrc 		 \
	     -Ithird_party       \
	     -Ithird_party/imgui \
	     -std=c++11          \
	     -Wall -Werror       \
	     -Wno-missing-braces \
	     -Wno-unused-function \
	     -Wno-unused-variable \
	     -Wno-unused-result \
	     -Wno-write-strings \
	     -Wno-c++11-compat-deprecated-writable-strings \
	     -fno-strict-aliasing \
	     -fno-omit-frame-pointer \
	     `pkg-config --cflags sdl2`

MLT_LDFLAGS = $(LDFLAGS) -lGL -lX11 -lXi `pkg-config --libs sdl2`

MLT_SRCS = src/canvas.cc 	src/canvas.h \
	   src/color.cc 	src/color.h \
	   src/milton.cc 	src/milton.h \
	   src/gui.cc 		src/gui.h \
	   src/memory.cc 	src/memory.h \
	   src/persist.cc 	src/persist.h \
	   src/profiler.cc 	src/profiler.h \
	   src/rasterizer.cc 	src/rasterizer.h \
	   src/sdl_milton.cc \
	   src/tests.cc 	src/tests.h \
	   src/utils.cc 	src/utils.h \
	   third_party/imgui/imgui_impl_sdl_gl3.cpp \
	   third_party/imgui/imgui_impl_sdl_gl3.h \
	   src/milton_configuration.h


all: build/milton

build/milton: build/headerlibs_impl.o $(MLT_SRCS)
	$(CC) $(MLT_CFLAGS) build/headerlibs_impl.o src/milton_unity_build.cc $(MLT_LDFLAGS) -o build/milton

build/headerlibs_impl.o: CFLAGS += -Ithird_party

build/headerlibs_impl.o: src/headerlibs_impl.cc build/.ok
	$(CC) $(CFLAGS) src/headerlibs_impl.cc -c -o build/headerlibs_impl.o && ar rcs build/headerlibs_impl.a build/headerlibs_impl.o


build/.ok:
	./scripts/pre_build.sh && touch build/.ok

clean:
	rm -rf build
