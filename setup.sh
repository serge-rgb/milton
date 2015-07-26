#!/bin/sh

if [ ! -d src/libnuwen ]; then
    git clone https://github.com/serge-rgb/libnuwen.git src/libnuwen
else
    pushd src/libnuwen
    git pull
    popd
fi

cd third_party/

echo "==== Installing dependencies"

PKGS="mercurial make cmake autoconf automake libtool clang "

PKGS_DEBIAN="$PKGS build-essential libasound2-dev libpulse-dev libaudio-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev \
    libxxf86vm-dev libxss-dev libgl1-mesa-dev libesd0-dev libdbus-1-dev libudev-dev \
    libgles1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev "

PKGS_ARCH="$PKGS base-devel libpulse libx11 libxext libxrandr libxcursor libxi \
    libxinerama libxxf86vm libxss mesa-libgl libdbus libsystemd mesa llvm "

if type "apt-get" &>/dev/null; then
    echo "==== APT found."
    sudo apt-get install $PKGS_DEBIAN
elif type "pacman" &>/dev/null; then
    echo "==== PACMAN found."
    sudo pacman -S --needed $PKGS_ARCH
else
    echo "Couldn't find installer. Exiting..."
    exit 1
fi
    

if [ ! -d build ]; then
    mkdir build
fi
cd build

cmake ../SDL2-2.0.3

make -j

cd ../..

