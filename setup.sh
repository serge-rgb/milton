#!/bin/sh
cd third_party/

echo "==== Installing dependencies ===="

PKGS="clang"

PKGS_DEBIAN="$PKGS libsdl2-dev"

PKGS_ARCH="$PKGS base-devel libsdl libpulse libx11 libxext libxrandr libxcursor libxi  libxinerama libxxf86vm libxss mesa-libgl libdbus libsystemd mesa llvm "

if type "apt-get" &>/dev/null; then
    sudo apt-get install $PKGS_DEBIAN

elif type "pacman" &>/dev/null; then
    sudo pacman -S --needed $PKGS_ARCH
else
    echo "****  I don't know the package manager for your distribution. Please install SDL2-dev manually ****"
    exit 1
fi

