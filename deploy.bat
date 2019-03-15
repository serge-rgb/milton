if exist OUTPUT goto OUTPUT_EXISTS

set builddir=build
set sdlbindir=third_party\bin

mkdir OUTPUT
copy %builddir%\Milton.exe OUTPUT\Milton.exe
copy %builddir%\Milton.pdb OUTPUT\Milton.pdb
copy Milton.iss OUTPUT\Milton.iss
copy %sdlbindir%\SDL2.lib OUTPUT\SDL2.lib
copy %sdlbindir%\SDL2.pdb OUTPUT\SDL2.pdb
copy milton_icon.ico OUTPUT\milton_icon.ico
copy LICENSE.txt OUTPUT\LICENSE.txt
copy %builddir%\Carlito.LICENSE OUTPUT\Carlito.LICENSE
copy %builddir%\Carlito.ttf OUTPUT\Carlito.ttf

mkdir OUTPUT\Standalone
copy OUTPUT\Milton.exe OUTPUT\Standalone\
copy OUTPUT\milton_icon.ico OUTPUT\Standalone\
copy OUTPUT\LICENSE.txt OUTPUT\Standalone\
copy OUTPUT\Carlito.ttf OUTPUT\Standalone\
copy OUTPUT\Carlito.LICENSE OUTPUT\Standalone\


goto end

:OUTPUT_EXISTS
echo "Error: Output exists."
exit /b 1
:end
exit /b 0