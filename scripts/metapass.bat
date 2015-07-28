@echo off

pushd src
cl metaprogram.c
if %errorlevel% neq 0 goto err
metaprogram.exe
goto end

:err

rem ERRORLEVEL = 1
(call)

:end
popd
