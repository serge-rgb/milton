@echo off


setlocal

set platform=x64

if "%1"=="x86" set platform=x86

set warnFlags=-FC
set includeFlags=-I..\third_party\SDL2-2.0.8\include -I..\third_party\imgui\ -I..\third_party

if not exist build mkdir build
pushd build
cl /Zi ..\src\shadergen.cc %warnFlags%
popd

taskkill /f /im milton.exe

build\shadergen.exe

pushd build

         copy "..\milton_icon.ico" "milton_icon.ico"
         copy "..\third_party\Carlito.ttf" "Carlito.ttf"
         copy "..\third_party\Carlito.LICENSE" "Carlito.LICENSE"
         copy ..\Milton.rc Milton.rc
         rc Milton.rc

set compiler_flags=/O2 /MTd /Zi %includeFlags% %warnFlags% /Femilton.exe /wd4217 /link ..\third_party\bin\%platform%\SDL2.lib OpenGL32.lib gdi32.lib shell32.lib comdlg32.lib ole32.lib oleAut32.lib winmm.lib advapi32.lib version.lib

if "%1"=="test" (
   cl ..\src\unity_tests.cc %compiler_flags /SUBSYSTEM:Console
) else (
   cl Milton.res ..\src\unity.cc %compiler_flags%
)
:: ..\third_party\bin\%platform%\SDL2.lib
popd
