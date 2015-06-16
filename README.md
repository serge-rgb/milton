Milton
======

Milton is a paint package. You know MS Paint? Like that.

How to Compile (Windows x64)
============================

1. Open a Windows shell. cd to Milton's directory
3. `scripts\vcvars.bat` to get Visual Studio's tools into the PATH. (Assumes VS is installed in the default dir.)
2. `run setup.bat` to clone and build dependencies; and to do a necessary meta-programming pass.
4. Open build\Milton.sln in VS 2013
5. build.bat is available if you want to buid from a shell or from your text editor.

To-Do
=====

* Bugs
    * :)

* Application
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

