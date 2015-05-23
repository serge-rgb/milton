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

To-Do
=====

* Bugs
    * :)

* Application
    * Center of scaling
    * Optimize: split view & reject strokes not in view
    * Optimize: Opacity early-reject.
    * ^--- freebie: Eraser
    * Optimize: Progressive rendering
    * Optimize: Threaded canvas update.
    * Basic wacom support
    * Gamma correct blending
    * Undo/redo
    * Basic saving / loading.
    * UI: Basic IM buttons

* Platforms
    * SDL version for Linux & Mac
    * Android NDK OpenGL hello world.

