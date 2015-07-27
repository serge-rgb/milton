@echo off

call scripts\metapass.bat

if %errorlevel% neq 0 goto fail

pushd build

cl /O2 /Oi /Zi /GR- /Gm- /W4 /Wall /fp:fast /nologo /wd4127 /wd4255 /wd4820 /wd4514 /wd4505 /wd4100 /wd4189 /wd4201 /wd4204 /wd4201 /wd4711 -D_CRT_SECURE_NO_WARNINGS /MT ..\src\win_milton.c  -I ..\third_party\ ..\third_party\glew32s.lib OpenGL32.lib user32.lib gdi32.lib /FeMilton.exe

if %errorlevel% neq 0 goto fail

rem Milton
goto end

:fail

set errorlevel=1

:end
set errorlevel=0
popd
