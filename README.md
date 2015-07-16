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
    * pan doesn't work when maximized in 'classic' windows mode.
    * re-add gamma correction with premultiplied alpha
    * :)

* Roadmap
    * Android: Port SDL
    * Fix color picker controls.
    * Basic saving / loading.
    * IMUI
    * SIMD-ify canvas rasterizer
    * SDL version for Linux & Mac
    * ? 64-bit canvas ?
    * ? iOS port ?

