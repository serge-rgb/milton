![MiltonLogo](http://i.imgur.com/hXxloIS.png)

Milton is a modern paint package.

It is intended to be a reinvention from first principles of what a program for painting should be.

Milton Paint is pre-alpha, currently in active development.

### [Latest release (2015-07-25) pre-alpha](https://github.com/serge-rgb/milton/releases/tag/prealpha001)

![alpaca](http://i.imgur.com/k7E8k7r.png)

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

    Milton currently works with a Wacom intuos pen&touch on Windows and Linux (X11).
    OSX support is coming.

- Gamma-correct alpha blending and 32-bit floating point per channel

- Software rendered.

- Fast and light-weight

- Multi-platform

    Windows, Linux, OSX

- Open Source

    Milton is licensed under the GPL


What Milton is not:
-------------------

Milton is not an image editor or a vector graphics editor. It's a program that
lets you draw, sketch and paint.

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

Milton targets Windows, Linux and OSX. 64 bit is recommended but not necessary.

Developed on:

* Windows 7 (Intel x64, NVidia card)
* Ubuntu 15.04 (same machine)
* OSX 10.10 Yosemite (Macbook Pro mid 2012)

Also known to work on:

* Arch Linux on a laptop with Intel integrated graphics

Windows
-------

Requirements:

- CMake (for SDL)
- Visual Studio 2013 (I'm using VS Ultimate with Surround Sound, but Community Edition should be fine)

0. Open a developer console. You have at least two options:
    - Open "Visual Studio Command Prompt" and go to Milton's directory.
    - Use cmd.exe and run `scripts\vcvars.bat` to have the Visual Studio suite in your PATH. It will try to use the 64-bit version
1. Run `setup.bat` to build SDL
2. `build.bat`
3. Milton is compiled to `build\Milton.exe`

Linux
-----

Requirements:

- SDL2 development libraries. On Ubuntu, this is `apt-get install libsdl2-dev`
- The clang compiler.

Note: If setup.sh fails, just install the equivalents of clang and libsdl-dev for your distribution.

1. `./build.sh`
2. Milton is compiled to `./milton`

OSX
---

Requirements:

- CMake (for building SDL)

1. `./setup_osx.sh` to download dependencies and build SDL
2. `./build_osx.sh`
3. Milton is compiled to `./milton`

Roadmap
-------

See TODO.txt

Thank You
=========

* Inspiration / Education
    * Casey Muratori. This program would be very different (and much slower) if not for Handmade Hero

* Art
    * Milton's logo by the very talented [Perla Fierro](http://portafolio.eclat-studio.com/)

* Code
    * [Michael Freundorfer](https://github.com/mordecai154)
    * [Joshua Mendoza](https://github.com/jomendoz)

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

