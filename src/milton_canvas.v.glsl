// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#version 120

attribute vec2 position;

// CanvasView elements:
uniform vec2 pan_vector;
uniform vec2 screen_center;
uniform int  scale;

void main()
{
    gl_Position.xy = position;
}

