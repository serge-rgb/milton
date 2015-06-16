@echo off

IF NOT EXIST src/libserg git clone https://github.com/bigmonachus/libserg.git src/libserg
gen_and_build
