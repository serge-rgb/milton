@echo off


setlocal

set platform=x64

if "%1"=="x86" set platform=x86

set warnFlags=
set includeFlags=-I..\third_party\SDL2-2.0.8\include -I..\third_party\imgui\ -I..\third_party

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

if "%1"=="test" (
   cl Milton.res ..\src\unity_tests.cc /MTd /Zi %includeFlags% %warnFlags% /FetestMilton.exe /wd4217 /link mincore.lib ..\third_party\bin\%platform%\SDL2.lib kernel32.lib OpenGL32.lib User32.lib gdi32.lib shell32.lib comdlg32.lib ole32.lib oleAut32.lib winmm.lib advapi32.lib version.lib
) else (
   cl Milton.res ..\src\unity.cc /MTd /Zi %includeFlags% %warnFlags% /Femilton.exe /wd4217 /link mincore.lib ..\third_party\bin\%platform%\SDL2.lib kernel32.lib OpenGL32.lib User32.lib gdi32.lib shell32.lib comdlg32.lib ole32.lib oleAut32.lib winmm.lib advapi32.lib version.lib
)
:: ..\third_party\bin\%platform%\SDL2.lib
popd
