Milton
======

Milton is a modern paint package.

It is intended to be a reinvention from first principles of what a program for painting should be.

Milton Paint is pre-alpha, currently in active development.

Features
--------

- Infinite canvas

    ![zoooom](http://i.imgur.com/fqOhPlr.gif)

    You don't pick a resolution, and you don't work with pixels.  Your work is
    stored as a sequence of commands, and rendered on the fly. Once it's
    implemented, you will be able to export your work to bitmaps of arbitrary
    size.

- Simple

    Milton solves a single problem: draw without pixels. It doesn't pretend to be
    something more than that. The UI will be simple, with a "no-manual-needed" motto.

- Persistent

    No save button. Ctrl-S is *so* 1980's

- Software rasterizer.

    Milton doesn't use the GPU to do the heavy lifting. This gives us the
    flexibility to implement more complex functionality than we would if we were
    using GPGPU. It also means a lot less headaches.

- Fast and light-weight

    Milton only consumes what it needs. It will squeeze all the juice it can from your
    CPU cores to make your experience as smooth as possible, but it is coded
    with care and love to be performant and efficient.

- Gamma-correct alpha blending and 32-bit floating point per channel

    You get the quality you would expect of a high-end product.

- Wacom support

    It currently has basic support for Wacom, there will be support for pressure
    sensitiviry and other good stuff

- Multi-platform

    Milton is developed on Windows, so that's what works right now; but there won't be an alpha until
    it works on OSX and Linux. Mobile support is planned

- Open Source

    Milton is licensed under the GPL


What Milton is not:
-------------------

Milton is not an image editor or a vector graphics editor. It's a program that lets you draw.

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

There's nothing to see here, just a GL triangle...

1. `cd android`
2. `gradle installAllDebug`


To-Do
=====

* Move this to github bug tracker :)

* Bugs
    * Windows: going to sleep sends an invalid size!
    * pan doesn't work when maximized in 'classic' windows mode.
    * :)

* Roadmap
    * Fix color picker controls.
    * Wacom: Support for pan & rotate.
    * Undo: Only redraw necessary area.
    * SDL version for Linux & Mac
    * android: Compile w/NDK
    * android: Draw canvas
    * ui: animated sprite.
    * ? iOS port ?

