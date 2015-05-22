
@echo off

pushd src

cl metaprogram.c /nologo
metaprogram

if errorlevel 1 goto end
popd

msbuild build\Milton.vcxproj /v:q


if errorlevel 1 goto end

echo Success

:end
