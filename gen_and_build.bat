
@echo off

pushd src

cl metaprogram.c /nologo
metaprogram

if errorlevel 1 goto end
popd

call build.bat

if errorlevel 1 goto end

echo Success

:end
