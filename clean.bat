@echo off
IF EXIST build rmdir /s /q build
del src\shaders.gen.h
echo ==== CLEAN ====
