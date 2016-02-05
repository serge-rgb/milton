@echo off

set sdl_done=0
set sdlmain_done=0

:sdl
echo "==== Building SDL ===="
pushd third_party
msbuild /maxcpucount SDL2-2.0.3\VisualC\SDL\SDL_VS2013.vcxproj /p:Configuration=Debug
if %errorlevel% == 0 set sdl_done=1
::msbuild /maxcpucount SDL2-2.0.3\VisualC\SDLmain\SDLmain_VS2013.vcxproj /p:Configuration=Debug
if %errorlevel% == 0 set sdlmain_done=1
popd

if %sdl_done% NEQ 1 goto error
if %sdlmain_done% NEQ 1 goto error


:ok
echo    [BUILD] SDL built.
goto end


:error
echo    [FATAL] -- Failed to build SDL
(call)

:end
