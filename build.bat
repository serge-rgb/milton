@echo off


setlocal

set platform=x64

if "%1"=="x86" set platform=x86

set unityBuild=1

set warnFlags=
set includeFlags=-I..\third_party\SDL2-2.0.8\include -I..\third_party\imgui\ -I..\third_party
set WindowsKitDir="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.15063.0\um\x64\\"

if not exist build mkdir build
pushd build
cl /Zi ..\src\shadergen.cc %warnFlags%
popd

build\shadergen.exe

pushd build

         copy "..\milton_icon.ico" "milton_icon.ico"
         copy "..\third_party\Carlito.ttf" "Carlito.ttf"
         copy "..\third_party\Carlito.LICENSE" "Carlito.LICENSE"
         copy ..\Milton.rc Milton.rc
         rc Milton.rc

cl Milton.res ..\src\unity.cc /MTd /Zi %includeFlags% %warnFlags% /Femilton.exe /wd4217 /link mincore.lib ..\third_party\bin\%platform%\SDL2.lib kernel32.lib OpenGL32.lib User32.lib gdi32.lib shell32.lib comdlg32.lib ole32.lib oleAut32.lib winmm.lib advapi32.lib version.lib
:: ..\third_party\bin\%platform%\SDL2.lib
popd
