// Copyright (c) 2019 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

in vec3 v_pointa;
in vec3 v_pointb;

void
main()
{
    vec2 screen_point = vec2(gl_FragCoord.x, u_screen_size.y - gl_FragCoord.y);

    vec2 canvas_point = raster_to_canvas_gl(screen_point);
    vec2 a = v_pointa.xy;
    vec2 b = v_pointb.xy;

    vec2 ab = b - a;
    float len_ab = length(ab);

    float t = clamp(dot((canvas_point - a)/len_ab, ab / len_ab), 0.0, 1.0);

    vec2 stroke_point = mix(a, b, t);

    float pressure = mix(v_pointa.z, v_pointb.z, t);

    // Distance between fragment and stroke
    float dist = distance(stroke_point, canvas_point);

    float rad = u_radius * pressure;
    out_color.r = dist / rad;
    out_color.a = 0.0f;
    if (dist < rad) {
        out_color.a = pressure;
    }
}