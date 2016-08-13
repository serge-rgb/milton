// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Note:
//  TODO: write documentation here about float<->int regarding strokes and milton
// TODO: use int?
attribute vec3 a_position;
attribute vec3 a_pointa;
attribute vec3 a_pointb;

varying vec3 v_pointa;
varying vec3 v_pointb;

#define MAX_DEPTH_VALUE  (16777216.0)	// (1<<24)


void main()
{
    v_pointa = a_pointa;
    v_pointb = a_pointb;
    gl_Position.xy = canvas_to_raster_gl(a_position.xy);
    gl_Position.z = a_position.z / MAX_DEPTH_VALUE;
}

