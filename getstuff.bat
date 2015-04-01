@echo off
IF NOT EXIST src/libserg git clone https://github.com/bigmonachus/libserg.git src/libserg

pushd src
cl metaprogram.c
metaprogram
popd
