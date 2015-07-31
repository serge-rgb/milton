Milton
======

Milton is a modern paint package.

It is intended to be a reinvention from first principles of what a program for painting should be.

Milton Paint is pre-alpha, currently in active development.

### [Latest release (2015-07-25) pre-alpha](https://github.com/serge-rgb/milton/releases/tag/prealpha001)

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

How to Compile
==============

Windows x64
-----------

Visual Studio 2013 needs to be installed

0. If needed, run `scripts\vcvars.bat` to have the Visual Studio suite in your PATH.
1. Run `setup.bat` to clone libNuwen, which is essentially the MIT-licensed part of Milton
2. Build with `build.bat`
3. Milton is compiled to `build\Milton.exe`

Workflow
--------

You can use build.bat to build from your favorite editor/IDE. If you want to
debug, I recommend doing `devenv build\Milton.exe` and hitting F11.


SDL (Linux and OSX)
-------------------

### Currently not working.

Currently only supporing Ubuntu or other Debian based systems. (Compiled on Ubuntu 15.04)

0. `./setup.sh`
1. `./build.sh`
2. Milton is compiled to `./milton`

Android
-------

There's nothing to see here, just a GL triangle...

1. `cd android`
2. `gradle installAllDebug`


Contributing
============
Constructive criticisim is always welcome; especially if it comes with a pull request!

Thank You
=========

* Inspiration / Education
    * Casey Muratori. This program would be very different (and much slower) if not for Handmade Hero

* Code
    * [Michael Freundorfer](https://github.com/mordecai154)
    * [Joshua Mendoza](https://github.com/jomendoz)

* Art & Feedback
    * [Perla Fierro](http://portafolio.eclat-studio.com/)

* Rubber-ducking / Whiteboarding
    * Rodrigo Gonzalez del Cueto [@rdelcueto](https://twitter.com/rdelcueto)
    * Luis Eduardo Pérez
    * Mom

* The "Jueves Sensual" team :)
    * Axel Becerril
    * Ruben Bañuelos
    * Caro Barberena
    * Carlos Chilazo
    * Roberto Lapuente
    * Joshua Mendoza
    * Maximiliano Monterrubio
    * Santiago Montesinos
    * Aarón Reyes García
    * Vane Ugalde
