@echo off

pushd build
call ..\scripts\metapass.bat
popd

msbuild build\Milton.sln /v:q

:end
