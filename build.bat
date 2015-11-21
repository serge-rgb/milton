@echo off

pushd build

set sdl_link_deps=Winmm.lib Version.lib Shell32.lib Ole32.lib OleAut32.lib Imm32.lib

REM NOTES:
REM     Define MILTON_FANCY_GL for OpenGL debugging messages.

set mlt_defines=-D_CRT_SECURE_NO_WARNINGS

set mlt_opt=/O2 /MT
set mlt_nopt=/Od /MTd

set mlt_compiler_flags=/Oi /Zi /GR- /Gm- /Wall /WX /fp:fast /nologo /FC /EHsc

REM 4100 Unreferenced func param (cleanup)
REM 4820 struct padding
REM 4255 () != (void)
REM 4668 Macro not defined. Subst w/0
REM 4710 Func not inlined
REM 4711 Auto inline
REM 4189 Init. Not ref
REM 4201 Nameless struct (GNU extension, but available in MSVC. Also, valid C11)
REM 4204 Non-constant aggregate initializer.
REM 4800 b32 to bool or int to bool perf warning
REM 4191 FARPROC from GetProcAddress
REM 5027 move assignment operator implicitly deleted
REM 4127 expression is constant. while(0) et al. Useful sometimes? Mostly annoying.
set comment_for_cleanup=/wd4100 /wd4189 /wd4800 /wd4127
set mlt_disabled_warnings=%comment_for_cleanup% /wd4820 /wd4255 /wd4668 /wd4710 /wd4711 /wd4201 /wd4204 /wd4191 /wd5027
set mlt_includes=-I ..\third_party\ -I ..\third_party\imgui -I ..\third_party\SDL2-2.0.3\include -I ..\..\EasyTab
set mlt_links=..\third_party\glew32s.lib OpenGL32.lib ..\third_party\SDL2-2.0.3\VisualC\SDL\x64\Debug\SDL2.lib ..\third_party\SDL2-2.0.3\VisualC\SDLmain\x64\Debug\SDL2main.lib user32.lib gdi32.lib %sdl_link_deps%

:: ---- Compile third_party libs with less warnings
cl %mlt_nopt% %mlt_includes% /Zi /c /EHsc ..\src\headerlibs_impl.cc
lib headerlibs_impl.obj

:: ---- Unity build for Milton
cl %mlt_nopt% %mlt_compiler_flags% %mlt_disabled_warnings% %mlt_defines% %mlt_includes%  ..\src\milton_unity_build.cc /FeMilton.exe %mlt_links% headerlibs_impl.lib
if %errorlevel% equ 0 goto ok
goto fail

goto ok
:fail
popd
exit /b 1

:ok
popd
exit /b 0
