@echo off


setlocal


REM NOTE: do not set this to 1 unless you're willing to change the hardcoded path below...
set unityBuild=1

set warnFlags=
set includeFlags=-Ithird_party/SDL2-2.0.3/include -Ithird_party/imgui/ -Ithird_party
set WindowsKitDir="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.15063.0\um\x64\\"


if "%unityBuild%"=="1" (
      pushd build
      cl /Zi src\shadergen.cc %warnFlags% -o shadergen.exe
      shadergen.exe
      cl src\unity.cc /Zi %includeFlags% %warnFlags% third_party\bin\SDL2.lib Gdi32.lib OpenGl32.lib Shell32.lib Comdlg32.lib  Ole32.lib  OleAut32.lib -o milton.exe
      popd
) else (
      del build\win64-msvc-debug-default\Milton.exe
      tundra\bin\tundra2.exe
      exit /b %errorlevel%
      )
