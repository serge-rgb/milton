// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Special values that RenderElement.count can take.
enum RenderElementType
{
    RenderElementType_LAYER = -1,
};

// Draw data for single stroke
struct RenderElement
{
    GLuint  vbo_stroke;
    GLuint  vbo_pointa;
    GLuint  vbo_pointb;

    i64     count;
    v4f     color;
    i32     radius;
};


