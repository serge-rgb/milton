@echo off

rem TODO: check if metaprogram has been modified and compile.

goto fsck_visual_studio

pushd ..\src
..\build\x64\Debug\Milton_metaprogram.exe
popd

:fsck_visual_studio

pushd src
cl metaprogram.c
metaprogram.exe
popd
