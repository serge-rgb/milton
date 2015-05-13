Milton
======

Milton is a paint package. You know MS Paint? Like that.

How to Compile (Windows x64)
============================

1. Open a Windows shell. cd to Milton's directory
2. `getstuff.bat` to clone dependencies
3. `scripts\vcvars.bat` to get Visual Studio's tools into the PATH. (Assumes VS is installed in the default dir.)
4. `gen_and_build.bat` to do run the meta-programming pass and then compile.
4. Open build\Milton.sln in VS 2013

How to Compile (Emscripten)
===========================

1. Run generate.sh (Note: Code Generation on OSX should be done with XCode's clang to avoid problems finding stdarg.h)
2. Make sure you have Emscripten (>= 1.30.0) set up
3. run `./build_web.sh`


To-Do
=====

* Bugs
    * libserg: meta expand produces null char.

* Application
    * Short-circuit. (ie. stop work so scaling is faster)
    * Smart full-fill stroke filtering to speed up scaling.
    * Panning
    * Use Emscripten to port to JS.
    * Eraser

* Crazy ideas
    * Progressive rendering

* UI
    * Figure out UI (Fork imgui? Write from scratch?)

* Web
    * Landing page.


