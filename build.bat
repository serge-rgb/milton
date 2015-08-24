@echo off

if %errorlevel% neq 0 goto fail

pushd build

set sdl_link_deps=Winmm.lib Version.lib Shell32.lib Ole32.lib OleAut32.lib Imm32.lib

cl /O2 /Oi /Zi /GR- /Gm- /Wall /WX /fp:fast /nologo /wd4127 /wd4255 /wd4820 /wd4514 /wd4505 /wd4100 /wd4189 /wd4201 /wd4204 /wd4201 /wd4711 /wd4668 -D_CRT_SECURE_NO_WARNINGS /MT ..\src\sdl_milton.c  -I ..\third_party\ -I ..\third_party\gui -I ..\third_party\SDL2-2.0.3\include ..\third_party\glew32s.lib OpenGL32.lib ..\third_party\SDL2-2.0.3\VisualC\SDL\x64\Debug\SDL2.lib ..\third_party\SDL2-2.0.3\VisualC\SDLmain\x64\Debug\SDL2main.lib %sdl_link_deps% user32.lib gdi32.lib /FeMilton.exe
if %errorlevel% equ 0 goto post_build
goto fail

:fail
rem the line below sets the errorlevel to 1
(call)
popd
:post_build
popd
