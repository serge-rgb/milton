@echo off

pushd build
..\scripts\metapass.bat
popd

msbuild build\Milton.vcxproj /v:q

:end
