// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

in vec3 v_pointa;
in vec3 v_pointb;

uniform sampler2D u_canvas;

in vec3 v_debug_color;

// TODO: This whole shader should be a variation in stroke_raster.f.glsl

void
main()
{
    vec2 screen_point = vec2(gl_FragCoord.x, u_screen_size.y - gl_FragCoord.y);

    vec2 canvas_point = raster_to_canvas_gl(screen_point);

    vec2 a = v_pointa.xy;
    vec2 b = v_pointb.xy;
    vec2 ab = b - a;
    float len_ab = length(ab);

    float t = clamp(dot(canvas_point - a, ab)/len_ab, 0.0, len_ab) / len_ab;
    vec2 stroke_point = mix(a, b, t);
    float pressure = mix(v_pointa.z, v_pointb.z, t);

    if ( distance(canvas_point, a) < u_radius*0.1 ) {
        out_color = vec4(v_debug_color, 1.0);
    } else {
        discard;
    }
}//END
