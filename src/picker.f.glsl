// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


// The triangle, in [-1,1].
uniform vec2 u_pointa;
uniform vec2 u_pointb;
uniform vec2 u_pointc;

uniform vec3 u_color;
uniform float u_angle;

uniform vec2 u_triangle_point;

uniform vec4 u_colors[5]; // Colors for picker buttons.

uniform vec2 u_screen_size;

in vec2 v_norm;


#define PI 3.14159

float
radians_to_degrees(float r)
{
    return (180.0 * r) / PI;
}

// http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
vec3
hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3
hsv_to_rgb(vec3 hsv)
{
    vec3 rgb;

    float h = hsv.x;
    float s = hsv.y;
    float v = hsv.z;
    float hh = h / 60.0;
    int hi = int(floor(hh));
    float cr = v * s;
    float rem = mod(hh, 2.0);
    float x = cr * (1.0 - abs(rem - 1.0));
    float m = v - cr;

    if( hi == 0 || hi == 6 ) {
        rgb.r = cr;
        rgb.g = x;
        rgb.b = 0;
    }
    if( hi == 1 ) {
        rgb.r = x;
        rgb.g = cr;
        rgb.b = 0;
    }
    if( hi == 2 ) {
        rgb.r = 0;
        rgb.g = cr;
        rgb.b = x;
    }
    if( hi == 3 ) {
        rgb.r = 0;
        rgb.g = x;
        rgb.b = cr;
    }
    if( hi == 4 ) {
        rgb.r = x;
        rgb.g = 0;
        rgb.b = cr;
    }
    if( hi == 5 ) {
        rgb.r = cr;
        rgb.g = 0;
        rgb.b = x;
    }
    rgb.r += m;
    rgb.g += m;
    rgb.b += m;

    return rgb;
}

// Could be called a signed area. `orientation(a, b, c) / 2` is the area of the
// triangle.
// If positive, c is to the left of ab. Negative: right of ab. 0 if
// colinear.
float
orientation(vec2 a, vec2 b, vec2 c)
{
    return (b.x - a.x)*(c.y - a.y) - (c.x - a.x)*(b.y - a.y);
}

bool
is_inside_triangle(vec2 p)
{
    bool is_inside =
           (orientation(u_pointa, u_pointb, p) <= 0) &&
           (orientation(u_pointb, u_pointc, p) <= 0) &&
           (orientation(u_pointc, u_pointa, p) <= 0);
    return is_inside;
}

void
main()
{
    // NOTE:
    //  These constants gotten from gui.cc in gui_init. From bounds_radius_px, wheel_half_width, and so on.
    float u_ui_scale = 1.75;
    float half_width = 12.0 / 100.0;
    float radius = (1.0 - (12.0 + 5) / 100.0);

    /* vec2 screen_point = vec2(gl_FragCoord.x, u_screen_size.y-gl_FragCoord.y); */
    /* vec2 coord = screen_point / u_screen_size; */
    /* coord.y = 1-coord.y; */
    vec4 color = vec4(0.5, 0.5, 0.55, 0.6);

    float dist = distance(vec2(0), v_norm);
    // Wheel and triangle
    // Show chosen color in preview circle
    vec2 preview_center = vec2(0.75,-0.75);
    const float preview_radius = 0.23;

    float dist_to_preview = distance(preview_center, v_norm);
    if ( dist_to_preview < preview_radius ) {
        color = vec4(0,0,0,1);
        const float epsilon = 0.02;
        if ( dist_to_preview < preview_radius - epsilon ) {
            color = vec4(u_color, 1);
        }
    }
    // else if ( dist < radius + half_width ) {
    //    color = vec4(0,0,0,1);
    // }
#if 1
    else if ( dist < radius+half_width ) {
        // Wheel
        if ( dist > radius-half_width ) {
            vec2 n = v_norm;
            vec2 v = vec2(1, 0);
            float angle = acos(dot(n, v)/ (length(n) * length(v)));
            if ( v_norm.y > 0 ) {
                angle = (2*PI) - angle;
            }
            vec3 hsv = vec3(radians_to_degrees(angle),1.0,1.0);
            //vec3 hsv = vec3(angle,1.0,1.0);
            vec3 rgb = hsv_to_rgb(hsv);
            color = vec4(rgb,1);
        }
        // Triangle
        else if ( is_inside_triangle(v_norm) ) {
            float area = orientation(u_pointa, u_pointb, u_pointc);
            float v = 1 - (orientation(v_norm, u_pointc, u_pointa) / area);
            float s = orientation(u_pointb, v_norm, u_pointa) * v / area;
            vec3 pure_color = hsv_to_rgb(vec3(u_angle,1.0,1.0));
            color = vec4((1.0-(1.0-pure_color)*s)*v,1.0);
        }
    }
#endif
    // Render buttons
    else if ( v_norm.y >= 1 ) {
        // Get the color for the rects
        int rect_i = int(((v_norm.x+1)/4) * 10);
        vec4 rect_color = u_colors[rect_i];
        color = rect_color;

        // Black outlines
        float h = ((v_norm.x+1)/4)*10;
        float epsilon = 0.01;
        float epsilon2 = 0.015;
        if ( v_norm.y > 1.4-epsilon
             || v_norm.y < 1+epsilon
             || (h <     epsilon2 && h >   - epsilon2)
             || (h < 1 + epsilon2 && h > 1 - epsilon2)
             || (h < 2 + epsilon2 && h > 2 - epsilon2)
             || (h < 3 + epsilon2 && h > 3 - epsilon2)
             || (h < 4 + epsilon2 && h > 4 - epsilon2)
             || (h < 5 + epsilon2 && h > 5 - epsilon2)
             || (h < 6 + epsilon2 && h > 6 - epsilon2) ) {
            color = vec4(0,0,0,1);
        }
    }

    float dist_to_choice = distance(v_norm, u_triangle_point);
    const float point_radius = 0.05;
    const float girth = 0.01;
    if ( dist_to_choice < point_radius+girth && dist_to_choice > point_radius-girth ) {
        color.rgb = vec3(1- color.r, 1 - color.g, 1 - color.b);
    }

    out_color = color;
}
