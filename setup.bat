@echo off

IF NOT EXIST build mkdir build
IF EXIST src\libserg goto pull
goto clone
:pull
pushd src\libserg
git pull
popd
goto sdl

:clone
IF NOT EXIST src/libserg git clone https://github.com/bigmonachus/libserg.git src/libserg

:sdl
echo "==== Building SDL"
pushd third_party
msbuild SDL2-2.0.3\VisualC\SDL_VS2013.sln
popd
echo "==== Finished building SDL2. Hopefully went OK"
@echo off

