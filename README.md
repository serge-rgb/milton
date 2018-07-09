![MiltonLogo](http://i.imgur.com/ADgRZUB.png)

[Milton](https://github.com/serge-rgb/milton) is an open source application that lets you Just Paint.

There are no pixels, you can paint with (almost) infinite detail. It feels raster-based but it works with vectors.
It is not an image editor. It is not a vector graphics editor. It is a program that lets you draw, sketch and paint.
There is no save button, your work is persistent with unlimited undo.

### [Latest release](https://github.com/serge-rgb/milton/releases/)

[![Stories in Ready](https://badge.waffle.io/serge-rgb/milton.png?label=ready&title=Ready)](https://waffle.io/serge-rgb/milton)
[![Join the chat at https://gitter.im/serge-rgb/milton](https://badges.gitter.im/serge-rgb/milton.svg)](https://gitter.im/serge-rgb/milton?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

![Milton Paint ss](http://i.imgur.com/4pdHeeI.png)

![zoooom](http://i.imgur.com/fqOhPlr.gif)


What Milton is not:
-------------------

Milton is not an image editor or a vector graphics editor. It's a program that
lets you draw, sketch and paint.

User Manual
===========

If the GUI makes something not-obvious, please create a github issue!

It's very helpful to drag the mouse (or pen) while pressing `space` to pan the
canvas.  Also, switching between the brush and the eraser with `b` and `e`.
You can change the brush size with `[` and `]` and control the transparency
with the number keys.

Here is the  [latest video tutorial](https://www.youtube.com/watch?v=g27gHio2Ohk)

Check out the [patreon page](https://www.patreon.com/serge_rgb?ty=h) if you would like to help out. :)

While on Windows there are binaries available, for Milton on Linux or OSX you will have to compile from source. There are some basic build instructions below. They will probably build, but please be prepared to do a bit of debugging on your end if you run into trouble, since these are not the primary development platforms.

How to Compile
==============

Windows
-------

Milton currently supports Visual Studio 2015.

Other versions of Visual Studio might not work.

To build:

Run `cmd.exe` and type the following

```
build.bat
```

Milton will be compiled to `build\win64-msvc-debug-default\Milton.exe`

Alternatively, to build with clang open build.bat and change the value of
doUnityClangBuild to 1. It assumes that you have LLVM and the Windows 10 SDK
installed.


Linux
-----

Work in progress, but is known to build and run.

### Dependencies
Use your linux distribution's package manager to install these.
* X11
* OpenGL
* XInput
* GTK2

### Building

Release build:
```sh
./build-lin.sh
build/Milton
```

build-lin.sh uses cmake under the hood, and any arguments you pass to it will be passed along to cmake.

Here are some CMake options you might care about:

| flag                  | type          | does                                                                                                                          |
| ----                  | ----          | ----                                                                                                                          |
| `TRY_GL2`             | `bool`        | Tells Milton to target OpenGL2.1. Does not guarantee that such a context will be created. This is the default Release target. |
| `TRY_GL3`             | `bool`        | Tells Milton to target OpenGL3.2. Does not guarantee that such a context will be created. This is the default Debug target.   |
| `CMAKE_BUILD_TYPE`    | `string`      | Configures the build type. Defaults to `Release`. Available build types are: `Release` and `Debug`.                           |


Example debug build u   sing GL2.1:
`./build-lin.sh -DCMAKE_BUILD_TYPE=Debug -DTRY_GL2=1`

OSX
---

Work in progress.

### Building
`./setup_osx.sh`

If you run into troubles building on OSX, please try to submit a pull request if possible.

License
=======

    Milton

    Copyright (C) 2015 - 2017 Sergio Gonzalez

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

Thanks
======

Milton is made with love by Sergio Gonzalez with the help of [awesome
people](https://github.com/serge-rgb/milton/blob/master/CREDITS.md).


