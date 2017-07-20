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

Milton is Windows only at the moment. Linux and OSX support is intended for the future, but I don't know when that will come.

How to Compile
==============

Windows
-------

Requirements: VS2015. Compiling with other versions of Visual Studio will
probably not work, since Milton provides a pre-compiled SDL.lib. If you want to
use another version of Visual Studio, you will need to compile SDL and copy
SDL.lib to the `build` directory.

To build:

Run `cmd.exe` and type the following

```
build.bat
```

Milton will be compiled to `build\Milton.exe`


Linux
-----

Porting in progress.

Build it with `make` and run it with `build/milton`. You need to install the SDL libraries (`libsdl2-dev` on Ubuntu).


OSX
---

_No OSX support at the moment._


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


