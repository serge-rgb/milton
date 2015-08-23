@echo off

IF NOT EXIST third_party\gui goto clone_gui
goto pull_gui
:clone_gui
git clone https://github.com/vurtun/gui.git third_party\gui
goto end_gui
:pull_gui
pushd third_party\gui
git pull
popd
:end_gui

IF NOT EXIST build mkdir build
:sdl
echo "==== Building SDL ===="
pushd third_party
msbuild /maxcpucount SDL2-2.0.3\VisualC\SDL\SDL_VS2013.vcxproj /p:Configuration=Debug
msbuild /maxcpucount SDL2-2.0.3\VisualC\SDLmain\SDLmain_VS2013.vcxproj /p:Configuration=Debug
popd
echo "==== Done ===="
:end
@echo off

