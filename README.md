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
    * Opacity early-reject.
    * Eraser
    * Tiled canvas render.
    * Basic saving / loading.
    * SIMD-ify canvas rasterizer
    * Threaded canvas update.
    * Tiled canvas (inifite panning)
    * Progressive rendering
    * ? Quadratic interpolation of strokes (intermediate points).
    * Gamma correct blending
    * UI: Basic IM buttons

* Platforms
    * SDL version for Linux & Mac
    * Android NDK OpenGL hello world
    * iOS GL hello world

