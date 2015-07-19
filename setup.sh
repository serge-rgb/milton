#!/bin/bash

if [ ! -d src/libserg ]; then
    git clone https://github.com/bigmonachus/libserg.git src/libserg
else
    pushd src/libserg
    git pull
    popd
fi

cd third_party/

echo "==== Installing dependencies (assuming Ubuntu or other Debian system)"
sudo apt-get install build-essential mercurial make cmake autoconf automake \
libtool libasound2-dev libpulse-dev libaudio-dev libx11-dev libxext-dev \
libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxxf86vm-dev \
libxss-dev libgl1-mesa-dev libesd0-dev libdbus-1-dev libudev-dev \
libgles1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev \
clang

if [ ! -d build ]; then
    mkdir build
fi
cd build

cmake ../SDL2-2.0.3

make -j

cd ../..

