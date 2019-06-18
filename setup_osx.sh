#!/bin/sh

echo "==== Generating build files ==== "
if [ ! -d build ]; then
mkdir build
fi
pushd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
popd

echo "==== Building dependencies ===="

cd third_party/


if [ ! -d build ]; then
mkdir build
fi

cd build
cmake ../SDL2-2.0.8
make -j
cd ../..



