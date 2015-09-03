#!/bin/sh
cd third_party/

echo "==== Installing dependencies ====\n"

echo "... memononen/nanovg"
if [ ! -d nanovg ]; then
    echo "    cloning"
    git clone https://github.com/memononen/nanovg
else
    cd nanovg
    git pull
    cd ..
fi

echo "...vurtun/gui"
if [ ! -d gui ]; then
    echo "    cloning"
    git clone https://github.com/vurtun/gui.git gui
else
    cd gui
    git pull
    cd ..
fi

echo "\n\n==== Done ====\n"
echo " Run ./build.sh to build Milton."
echo "   Please make sure that you have a development version of SDL2 installed. On"
echo "   Ubuntu or other Debian-based distros, this is libsdl2-dev. \n\n"

# PKGS="clang"

# PKGS_DEBIAN="$PKGS libsdl2-dev"

# PKGS_ARCH="$PKGS base-devel libsdl libpulse libx11 libxext libxrandr libxcursor libxi  libxinerama libxxf86vm libxss mesa-libgl libdbus libsystemd mesa llvm "

# if type "apt-get" &>/dev/null; then
#     sudo apt-get install $PKGS_DEBIAN

# elif type "pacman" &>/dev/null; then
#     sudo pacman -S --needed $PKGS_ARCH
# else
#     echo "****  I don't know the package manager for your distribution. Please install SDL2-dev manually ****"
#     exit 1
# fi

