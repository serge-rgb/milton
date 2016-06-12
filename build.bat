@echo off

:: ---- Build type
::
:: - 1: Optimized build.
:: - 0: Debug build.
set mlt_opt_level=0

IF EXIST  build\ctime.exe ( set has_ctime=1 ) ELSE ( set has_ctime=0 )

IF %has_ctime%==1 build\ctime.exe -begin milton.ctm

IF NOT EXIST build mkdir build

IF NOT EXIST build\SDL2.lib copy third_party\bin\SDL2.lib build\SDL2.lib
IF NOT EXIST build\SDL2.pdb copy third_party\bin\SDL2.pdb build\SDL2.pdb
IF NOT EXIST build\milton_icon.ico copy milton_icon.ico build\milton_icon.ico

pushd build
set sdl_link_deps=Winmm.lib Version.lib Shell32.lib Ole32.lib OleAut32.lib Imm32.lib

set mlt_defines=-D_CRT_SECURE_NO_WARNINGS

:: ---- Define opt & nopt flags
:: Oy- disable frame pointer omission (equiv. to -f-no-omit-frame-pointer)
set mlt_opt=/Ox /Oy- /MT
set mlt_nopt=/Od /MT
:: both are /MT to match static SDL. Live and learn

if %mlt_opt_level% == 0 set mlt_opt_flags=%mlt_nopt%
if %mlt_opt_level% == 1 set mlt_opt_flags=%mlt_opt%


set mlt_compiler_flags=/Oi /Zi /GR- /Gm- /Wall /WX /nologo /FC /EHsc
:: Build profiling:
:: /Bt+

REM 4100 Unreferenced func param (cleanup)
REM 4820 struct padding
REM 4255 () != (void)
REM 4710 Func not inlined
REM 4711 Auto inline
REM 4189 Init. Not ref
REM 4201 Nameless struct (GNU extension, but available in MSVC. Also, valid C11)
REM 4204 Non-constant aggregate initializer.
REM 4800 b32 to bool or int to bool perf warning
REM 4191 FARPROC from GetProcAddress
REM 5027 move assignment operator implicitly deleted
REM 4127 expression is constant. while(0) et al. Useful sometimes? Mostly annoying.
REM 4514 unreferenced inline function removed
REM 4305 truncate T to bool
REM 4242 u64 to size_t (VS2015 dev prompt)
REM 4244 u64 to size_t (VS2015 dev prompt)
REM 4738 32bit float result in memory (VS2015 dev prompt)
REM 4668 not defined, replacing as 0
REM 4239 non-standard extension conversion from ivec3 to ivec2&
set comment_for_cleanup=/wd4100 /wd4189 /wd4800 /wd4127 /wd4668 /wd4239
set mlt_disabled_warnings=%comment_for_cleanup% /wd4305 /wd4820 /wd4255 /wd4710 /wd4711 /wd4201 /wd4204 /wd4191 /wd5027 /wd4514 /wd4242 /wd4244 /wd4738
set mlt_includes=-I ..\third_party\ -I ..\third_party\imgui -I ..\third_party\SDL2-2.0.3\include -I ..\..\EasyTab -I ..\third_party\nativefiledialog\src\include

::set sdl_dir=..\third_party\SDL2-2.0.3\VisualC\SDL\x64\Debug
set sdl_dir=..\third_party\bin

:: shell32.lib -- ShellExcecute to open help
set mlt_link_flags=OpenGL32.lib user32.lib gdi32.lib Comdlg32.lib Shell32.lib /SAFESEH:NO /DEBUG

if NOT %has_ctime%==1 cl /Ox ..\third_party\ctime.c winmm.lib

IF NOT EXIST Milton.rc copy ..\Milton.rc Milton.rc && rc /r Milton.rc
IF NOT EXIST Carlito.LICENSE copy ..\third_party\Carlito.LICENSE
IF NOT EXIST Carlito.ttf copy ..\third_party\Carlito.ttf


echo    [BUILD] -- Building Milton...

:: ---- Unity build for Milton

cl %mlt_opt_flags% %mlt_compiler_flags% %mlt_disabled_warnings% %mlt_defines% %mlt_includes% /c  ..\src\milton_unity_build.cc
if %errorlevel% neq 0 goto fail

link milton_unity_build.obj /OUT:Milton.exe %mlt_link_flags%  %sdl_link_deps% SDL2.lib Milton.res
if %errorlevel% neq 0 goto fail

:ok
echo    [BUILD]  -- Build success!
popd
goto end

:fail
echo    [FATAL] -- ... error building Milton
popd && (call)

:end
if %has_ctime%==1 build\ctime -end milton.ctm
