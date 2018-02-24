// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

in vec2 a_point;
in vec2 a_uv;
out vec2 v_uv;

void
main()
{
    v_uv = a_uv;
    gl_Position = vec4(a_point, 0,1);
}
