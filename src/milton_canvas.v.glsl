// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Note:
//  TODO: write documentation here about float<->int regarding strokes and milton
attribute vec2 a_position;


void main()
{
    gl_Position.xy = canvas_to_raster_gl(a_position);
}

