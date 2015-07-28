@echo off

IF NOT EXIST build mkdir build
IF EXIST src\libnuwen goto pull
goto clone
:pull
pushd src\libnuwen
git pull
popd
goto sdl

:clone
IF NOT EXIST src/libnuwen git clone https://github.com/serge-rgb/libnuwen.git src/libnuwen

:sdl
echo "==== Building SDL"
pushd third_party
msbuild /maxcpucount SDL2-2.0.3\VisualC\SDL\SDL_VS2013.vcxproj /p:Configuration=Debug
msbuild /maxcpucount SDL2-2.0.3\VisualC\SDLmain\SDLmain_VS2013.vcxproj /p:Configuration=Debug
popd
echo "==== Done"
:end
@echo off

