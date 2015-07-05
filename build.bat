@echo off

rem ==== This is a quick'n dirty script to compile Milton with cl.exe                   ====
rem ==== The full Visual Studio solution must be built at least once for this to work   ====
rem ==== However, this is great to quickly compile from your favorite editor            ====
rem ==== because it's faster than calling msbuild...                                    ====
rem ====                                                                                ====
rem ==== Shouldn't be used when messing with metaprogramming.                           ====

pushd build
call ..\scripts\metapass.bat

cl /nologo /O2 /wd4127 /wd4255 /wd4820 /wd4514 /wd4505 /wd4100 /wd4189 /wd4201 /wd4204 /wd4201 /wd4711 -D_CRT_SECURE_NO_WARNINGS /Wall /MT ..\src\win_milton.c  -I ..\third_party\ -I ..\third_party\SDL2-2.0.3\include\ ..\third_party\glew32s.lib ..\third_party\SDL2-2.0.3\VisualC\SDL\x64\Release\SDL2.lib OpenGL32.lib user32.lib gdi32.lib

popd

:end
