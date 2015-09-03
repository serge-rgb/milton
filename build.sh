echo "1..."
cd src
clang template_expand.c -g -o template_expand
te_ok=$?
./template_expand
cd ..


if [ $te_ok -ne 0 ]; then
    echo "Error in meta-programming step."
    exit 1
fi

# Look for sdl
pkg-config --exists sdl2
sdl_ok=$?


if [ $sdl_ok -eq 0 ] && [ $? -eq 0 ]; then
    echo "2..."
    # Omit -Wno-unused-(variable|function) to clean up code
    clang -Ithird_party \
	-std=c99\
	-Wall -Werror \
	-Wno-missing-braces \
	-Wno-unused-function \
	-Wno-unused-variable \
	-Wno-unused-result \
	-fno-strict-aliasing \
	`pkg-config --cflags sdl2` \
	-O2 -g\
	src/sdl_milton.c -lGL -lm \
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
