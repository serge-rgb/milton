@echo off

if %errorlevel% neq 0 goto fail


REM ---- Delete old generated files.
del src\*.generated.h

pushd src
cl /Zi template_expand.c
template_expand.exe
popd

pushd build

set sdl_link_deps=Winmm.lib Version.lib Shell32.lib Ole32.lib OleAut32.lib Imm32.lib

REM NOTES:
REM     Define MILTON_FANCY_GL for OpenGL debugging messages.

set mlt_defines=-D_CRT_SECURE_NO_WARNINGS

set mlt_opt=/O2 /MT
set mlt_nopt=/Od /MTd /DNDEBUG

set mlt_compiler_flags=/Oi /Zi /GR- /Gm- /Wall /WX /fp:fast /nologo
set mlt_disabled_warnings=/wd4127 /wd4255 /wd4820 /wd4514 /wd4505 /wd4100 /wd4189 /wd4201 /wd4204 /wd4201 /wd4711 /wd4668 /wd4710

set mlt_includes=-I ..\third_party\ -I ..\third_party\gui -I ..\third_party\SDL2-2.0.3\include

set mlt_links=..\third_party\glew32s.lib OpenGL32.lib ..\third_party\SDL2-2.0.3\VisualC\SDL\x64\Debug\SDL2.lib ..\third_party\SDL2-2.0.3\VisualC\SDLmain\x64\Debug\SDL2main.lib user32.lib gdi32.lib %sdl_link_deps%

cl %mlt_opt% %mlt_compiler_flags% %mlt_disabled_warnings% %mlt_defines% %mlt_includes%  ..\src\milton_unity_build.c /FeMilton.exe %mlt_links%
if %errorlevel% equ 0 goto post_build
goto fail

:fail
rem the line below sets the errorlevel to 1
(call)
popd
:post_build
popd
