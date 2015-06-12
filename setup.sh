if [ ! -d src/libserg ]; then
    git clone https://github.com/bigmonachus/libserg.git src/libserg
fi

pushd src

clang metaprogram.c -g -o metaprogram
./metaprogram

popd
