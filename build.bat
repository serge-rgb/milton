@echo off

call scripts\metapass.bat

if %errorlevel% neq 0 goto fail

pushd build

cl /Od /Oi /Zi /GR- /Gm- /W4 /Wall /fp:fast /nologo /wd4127 /wd4255 /wd4820 /wd4514 /wd4505 /wd4100 /wd4189 /wd4201 /wd4204 /wd4201 /wd4711 -D_CRT_SECURE_NO_WARNINGS /MT ..\src\win_milton.c  -I ..\third_party\ -I ..\third_party\SDL2-2.0.3\include\ ..\third_party\glew32s.lib ..\third_party\SDL2-2.0.3\VisualC\SDL\x64\Debug\SDL2.lib OpenGL32.lib user32.lib gdi32.lib

if %errorlevel% neq 0 goto fail

win_milton

:fail

set errorlevel=1

:end
set errorlevel=0
popd
