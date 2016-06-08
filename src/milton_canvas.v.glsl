// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#version 120

attribute vec2 position;

void main()
{
    gl_Position.xy = position;
}

