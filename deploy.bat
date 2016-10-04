if exist OUTPUT goto OUTPUT_EXISTS

mkdir OUTPUT
copy build\Milton.exe OUTPUT\Milton.exe
copy build\Milton.pdb OUTPUT\Milton.pdb
copy Milton.iss OUTPUT\Milton.iss
copy build\SDL2.lib OUTPUT\SDL2.lib
copy build\SDL2.pdb OUTPUT\SDL2.pdb
copy milton_icon.ico OUTPUT\milton_icon.ico
copy LICENSE.txt OUTPUT\LICENSE.txt
copy build\Carlito.LICENSE OUTPUT\Carlito.LICENSE
copy build\Carlito.ttf OUTPUT\Carlito.ttf

goto end

:OUTPUT_EXISTS
echo "Error: Output exists."
exit /b 1
:end
exit /b 0