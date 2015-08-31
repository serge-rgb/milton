#!/bin/sh

cd third_party/

echo "==== Installing dependencies ===="

if [ ! -d gui ]; then
    git clone https://github.com/vurtun/gui.git gui
else
    cd gui
    git pull
    cd ..
fi

if [ ! -d build ]; then
mkdir build
fi

cd build
cmake ../SDL2-2.0.3
make -j
cd ../..


