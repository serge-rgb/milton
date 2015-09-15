#!/bin/sh

cd third_party/

echo "==== Installing dependencies ===="

if [ ! -d build ]; then
mkdir build
fi

cd build
cmake ../SDL2-2.0.3
make -j
cd ../..


