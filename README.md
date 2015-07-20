Milton
======

Milton is a paint package. You know MS Paint? Like that.

How to Compile (Windows x64)
============================

Requirements
------------
Visual Studio 2013 needs to be installed

0. If needed, run `scripts\vcvars.bat` to have the Visual Studio suite in your PATH.
1. Run `setup.bat` to clone dependencies and build SDL.
2. Build with `build.bat`

Workflow
--------

You can use build.bat to build from your favorite editor/IDE. If you want to
debug, I recommend doing `devenv build\win_milton.exe` and hitting F11.

Android
=======
Currently keeping up with Google moving everything to Gradle & Android Studio.

1. `cd android`
2. `gradle installAllDebug`


To-Do
=====

* Bugs
    * Windows: going to sleep sends an invalid size to milton!
    * [WONT-FIX] pan doesn't work when maximized in 'classic' windows mode.
    * :)

* Roadmap
    * Wacom: Support for pan & rotate.
    * Undo: Only redraw necessary area.
    * SDL version for Linux & Mac
    * SIMD-ify canvas rasterizer (SSE)
    * android: Compile w/NDK
    * android: Draw canvas
    * Fix color picker controls.
    * ui: animated sprite.
    * ? iOS port ?

