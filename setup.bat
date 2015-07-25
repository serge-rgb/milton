@echo off

IF NOT EXIST build mkdir build
IF EXIST src\libnuwen goto pull
goto clone
:pull
pushd src\libnuwen
git pull
popd
goto end

:clone
IF NOT EXIST src/libnuwen git clone https://github.com/serge-rgb/libnuwen.git src/libnuwen

:end
@echo off

