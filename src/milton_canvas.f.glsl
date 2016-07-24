// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// TODO: this layout qualifier introduces GLSL 150 dependency.
layout(origin_upper_left) in vec4 gl_FragCoord;

flat in vec3 v_pointa;
flat in vec3 v_pointb;

uniform sampler2D u_canvas;


#define PRESSURE_RESOLUTION_GL (1<<20)

bool brush_is_eraser()
{
    bool is_eraser = false;
    if (u_brush_color == vec4(23,34,45,56)) // defined in canvas.cc k_eraser_color
    {
        is_eraser = true;
    }
    return is_eraser;
}


vec4 blend(vec4 dst, vec4 src)
{
    vec4 result = src + dst*(1.0f-src.a);

    return result;
}

// x,y  - closest point
// z    - t in [0,1] interpolation value
vec3 closest_point_in_segment_gl(vec2 a, vec2 b,
                                 vec2 ab, float ab_magnitude_squared,
                                 vec2 point)
{
    vec3 result;
    float mag_ab = sqrt(ab_magnitude_squared);
    float d_x = ab.x / mag_ab;
    float d_y = ab.y / mag_ab;
    float ax_x = float(point.x - a.x);
    float ax_y = float(point.y - a.y);
    float disc = d_x * ax_x + d_y * ax_y;

#if 0
    if (disc < 0.0)
    {
        disc = 0.0;
    }
    else if (disc > mag_ab)
    {
        disc = mag_ab;
    }
#else
    float ltz = float(disc < 0.0);
    disc = ltz*0.0 + (1-ltz)*disc;
    float gt = float(disc > mag_ab);
    disc = gt*mag_ab + (1-gt)*disc;
#endif
    result = VEC3(a.x + disc * d_x, a.y + disc * d_y, disc / mag_ab);
    return result;
}

#define MAX_DIST (1<<24)

void main()
{
// Note: this doesn't seem to help at all on the GPU!
#if 0
    vec2 coord = gl_FragCoord.xy / u_screen_size;
    coord.y = 1-coord.y;
    vec4 color = texture2D(u_canvas, coord);
    if (color.a == 1) { discard; }
#endif

    vec2 fragment_point = raster_to_canvas_gl(gl_FragCoord.xy);
    bool found = false;
    vec3 a = v_pointa;
    vec3 b = v_pointb;

    vec2 ab = b.xy - a.xy;
    float ab_magnitude_squared = ab.x*ab.x + ab.y*ab.y;
    if (ab_magnitude_squared > 0)
    {
        vec3 stroke_point = closest_point_in_segment_gl(a.xy, b.xy, ab, ab_magnitude_squared, fragment_point);
        float d = distance(stroke_point.xy, fragment_point);
        float t = stroke_point.z;
        float pressure_a = a.z;
        float pressure_b = b.z;
        float pressure = (1-t)*pressure_a + t*pressure_b;
        pressure /= float(PRESSURE_RESOLUTION_GL);
        float radius = pressure * u_radius;
        bool inside = d < radius;
        if (inside)
        {
            //color = brush_is_eraser() ? blend(color, vec4(u_background_color, 1)) : blend(color, u_brush_color);
            // If rendering front-to-back, with screen cleared:
            //color = brush_is_eraser() ? blend(vec4(u_background_color, 1), color) : blend(u_brush_color, color);
            found = true;
        }
    }
    if (found)
    {
        gl_FragColor = vec4(1);
    }
    else
    {
        discard;
    }
}
//End
