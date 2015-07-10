@echo off

pushd src
cl metaprogram.c
if %errorlevel% neq 0 goto err
metaprogram.exe
goto end

:err

set errorlevel=1

:end
popd
