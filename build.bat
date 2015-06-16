@echo off

pushd build
call ..\scripts\metapass.bat
popd

msbuild build\Milton.vcxproj /v:q

:end
