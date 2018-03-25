@echo off


setlocal


REM NOTE: do not set this to 1 unless you're willing to change the hardcoded path below...
set doUnityClangBuild=0

set warnFlags=-Wno-writable-strings -Wno-format-security -Wno-deprecated-declarations -Wno-microsoft-include
set includeFlags=-Ithird_party/SDL2-2.0.3/include -Ithird_party/imgui/ -Ithird_party
set WindowsKitDir="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.15063.0\um\x64\\"


if "%doUnityClangBuild%"=="1" (
			       clang-cl /Zi src\shadergen.cc %warnFlags% -o shadergen.exe
			       shadergen.exe
			       clang-cl src\unity.cc /Zi %includeFlags% %warnFlags% third_party\bin\SDL2.lib %WindowsKitDir%Gdi32.lib %WindowsKitDir%OpenGl32.lib %WindowsKitDir%Shell32.lib %WindowsKitDir%Comdlg32.lib  %WindowsKitDir%Ole32.lib  %WindowsKitDir%OleAut32.lib -o milton.exe

) else (
      del build\win64-msvc-debug-default\Milton.exe
      tundra\bin\tundra2.exe
      exit /b %errorlevel%
      )
