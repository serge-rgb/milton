@echo off

IF EXIST src\libserg goto pull
goto clone
:pull
pushd src\libserg
git pull
popd

:clone
IF NOT EXIST src/libserg git clone https://github.com/bigmonachus/libserg.git src/libserg

