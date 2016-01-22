
# Look for sdl
pkg-config --exists sdl2
sdl_ok=$?

if [ $sdl_ok -ne 0 ]; then
    echo "Pkg config doesn't think you have SDL 2 installed."
    (exit 1)
else
    if [ ! -d build ]; then
        echo "Creating build directory ./build"
        mkdir build
    fi
fi

