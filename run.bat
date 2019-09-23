REM  this script tries to build the program and if successful it runs it and then
REM  prints everything that was output to stdout by the program (e.g. with printf())

cls & call build.bat && build\milton.exe>output.txt && type output.txt
