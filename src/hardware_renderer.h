// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Draw data for single stroke
struct RenderElement
{
    GLuint  vbo_stroke;
    GLuint  vbo_pointa;
    GLuint  vbo_pointb;
    GLuint  indices;

    i64     count;
    v4f     color;
    i32     radius;

    int     flags;  // RenderElementFlags enum;
};


