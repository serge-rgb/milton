![MiltonLogo](http://i.imgur.com/hXxloIS.png)

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
scripts\vcvars.bat
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

    Copyright (c) 2015-2016 Sergio Gonzalez

    Permission is hereby granted, free of charge, to any person obtaining a copy of
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is furnished to do
    so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

Credits and Thanks
==================

* Art
    * Milton's logo and artwork by the very talented [Perla Fierro](http://portafolio.eclat-studio.com/)

* Code
    * [Apoorva Joshi](http://apoorvaj.io) - [EasyTab](https://github.com/ApoorvaJ/EasyTab).
    * [Mio Iwakura](http://miotatsu.github.io) - Linux version

    * [Charly Mourglia](http://github.com/Zouch)
    * [Joshua Mendoza](https://github.com/jomendoz)
    * [Kevin Wojniak](https://github.com/kainjow) - macOS tundra build
    * [Michael Freundorfer](https://github.com/mordecai154)
    * [Tilmann Rübbelke](https://github.com/TilmannR) - Color picker fixes & improvements

* Other
    * Aarón Reyes García
    * Axel Becerril
    * Carlos Chilazo
    * Caro Barberena
    * Joshua Mendoza
    * Luis Eduardo Pérez
    * Maximiliano Monterrubio
    * Mom
    * Roberto Lapuente
    * Rodrigo Gonzalez del Cueto [@rdelcueto](https://twitter.com/rdelcueto)
    * Ruben Bañuelos
    * Santiago Montesinos
    * Vane Ugalde

* Inspiration / Education
    * Casey Muratori for Handmade Hero

Glorious Patrons of the Coding Craft
------------------------------------

Thank you so much for supporting Milton on [Patreon](https://www.patreon.com/serge_rgb?ty=h):

* [Neil Blakey-Milner](https://www.patreon.com/nxsy)
* [Pau Fernández](https://twitter.com/pauek)
* [Maria Lopez-Lacroix](https://amacasera.com/)
* [Jeroen van Rijn](https://twitter.com/J_vanRijn)
