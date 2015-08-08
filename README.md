Milton
======

Milton is a modern paint package.

It is intended to be a reinvention from first principles of what a program for painting should be.

Milton Paint is pre-alpha, currently in active development.

### [Latest release (2015-07-25) pre-alpha](https://github.com/serge-rgb/milton/releases/tag/prealpha001)

My latest masterpiece: An alpaca

![alpaca](http://i.imgur.com/fJJZ0Bj.png)

![alpacaGif](http://i.imgur.com/QR8TPDJ.gif)


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

- Wacom support

    It currently has basic support for Wacom, there will be support for pressure
    sensitivity and other good stuff

- Gamma-correct alpha blending and 32-bit floating point per channel

- Software rendered.

- Fast and light-weight

- Multi-platform

    Windows, Linux, OSX

- Open Source

    Milton is licensed under the GPL


What Milton is not:
-------------------

Milton is not an image editor or a vector graphics editor. It's a program that lets you draw.

User Manual
===========

While we get the GUI situation figured out, you can use the keyboard.

Keyboard Shortcuts
------------------

| Input                  | Action                |
| :--------------------: | --------------------: |
| `Space + Mouse Drag`   | Drag Canvas           |
| `0 - 9`                | Change brush opacity  |
| `[`                    | Make brush smaller    |
| `]`                    | Make brush larger     |
| `ctrl + z`             | Undo                  |
| `ctrl + y`             | Redo                  |
| `ctrl + backspace`     | Clear (not undoable!) |

Your work is kept in a file called MiltonPersist.mlt
Support for multiple files is coming. But right now you can just pan the canvas

How to Compile
==============

Milton targets Windows, Linux and OSX for 64 bit machines.

Developed on:

* Windows 7 (Intel x64, NVidia card)
* Ubuntu 15.04 (same machine)
* OSX 10.10 Yosemite (Macbook Pro mid 2012)

Also known to work on:

* Arch Linux on a laptop with Intel integrated graphics

There is a chance that the master branch will not build on one of the platforms
because of some warning, but if Milton builds and fails to work on your
machine, please let me know.

Windows x64
-----------

Requirements:

- CMake (for SDL)
- Visual Studio 2013 (I'm using VS Ultimate with Surround Sound, but Community Edition should be fine)

0. If needed, run `scripts\vcvars.bat` to have the Visual Studio suite in your PATH.
1. Run `setup.bat` to clone libnuwen, which is essentially the MIT-licensed part of Milton, and build SDL
2. Build with `build.bat`
3. Milton is compiled to `build\Milton.exe`

Linux
-----

The setup script will try and get libsdl. I'm on Ubuntu and a friend added Arch. Patches welcome!

0. `./setup.sh`
1. `./build.sh`
2. Milton is compiled to `./milton`

OSX
---

Requirements:

- CMake (for building SDL)

0. `./setup_osx.sh` to build SDL
1. `./build_osx.sh` to build milton
2. Milton is compiled to `./milton`

Problems compiling?
-------------------

If Milton doesn't compile. First thing to try is to run the setup script to get
libnuwen up-to-date. This bites me all the time, so a better solution is
needed. Git submodules are terrifying.

Workflow
--------

On Windows, You can use build.bat to build from your favorite editor/IDE. If you want to
debug, I recommend doing `devenv build\Milton.exe` and hitting F11.

On Linux, use build.sh and your favorite and/or least horrible debugger.
On OSX, creating an XCode project for debugging works pretty well.

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

