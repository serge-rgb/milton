#version 150

// The triangle, in [-1,1].
uniform vec2 u_pointa;
uniform vec2 u_pointb;
uniform vec2 u_pointc;

uniform vec2 u_center;
uniform float u_half_width;
uniform float u_radius;
uniform float u_angle;

uniform vec3 u_colors[5]; // Colors for picker buttons.

uniform sampler2D u_canvas;  // The canvas FBO, to blend the picker in

in vec2 v_norm;

#define PI 3.14159

float radians_to_degrees(float r)
{
    return (180.0 * r) / PI;
}

// http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 hsv_to_rgb(vec3 hsv)
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

    switch (hi)
    {
    case 0:
        rgb.r = cr;
        rgb.g = x;
        rgb.b = 0;
        break;
    case 1:
        rgb.r = x;
        rgb.g = cr;
        rgb.b = 0;
        break;
    case 2:
        rgb.r = 0;
        rgb.g = cr;
        rgb.b = x;
        break;
    case 3:
        rgb.r = 0;
        rgb.g = x;
        rgb.b = cr;
        break;
    case 4:
        rgb.r = x;
        rgb.g = 0;
        rgb.b = cr;
        break;
    case 5:
        rgb.r = cr;
        rgb.g = 0;
        rgb.b = x;
        break;
    default:
        break;
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
float orientation(vec2 a, vec2 b, vec2 c)
{
    return (b.x - a.x)*(c.y - a.y) - (c.x - a.x)*(b.y - a.y);
}

bool is_inside_triangle(vec2 p)
{
    bool is_inside =
           (orientation(u_pointa, u_pointb, p) <= 0) &&
           (orientation(u_pointb, u_pointc, p) <= 0) &&
           (orientation(u_pointc, u_pointa, p) <= 0);
    return is_inside;
}

void main()
{
    // NOTE:
    //  These constants gotten from gui.cc in gui_init. From bounds_radius_px, wheel_half_width, and so on.
    float half_width = 12.0 / 100.0;
    float radius = 1.0 - (12.0 + 5) / 100.0;


    vec4 color = vec4(0.5,0.5,0.55,0.4);
    // Wheel and triangle

    float dist = distance(vec2(0), v_norm);
    if (dist < radius+half_width)
    {
        // Wheel
        if (dist > radius-half_width)
        {
            vec2 n = v_norm;
            vec2 v = vec2(1, 0);
            float angle = acos(dot(n, v)/ (length(n) * length(v)));
            if (v_norm.y > 0)
            {
                angle = (2*PI) - angle;
            }
            vec3 hsv = vec3(radians_to_degrees(angle),1.0,1.0);
            //vec3 hsv = vec3(angle,1.0,1.0);
            vec3 rgb = hsv_to_rgb(hsv);
            color = vec4(rgb,1);
        }
        else if (is_inside_triangle(v_norm))
        {
            float area = orientation(u_pointa, u_pointb, u_pointc);
            float inv_area = 1.0f / area;
            float s = orientation(u_pointb, v_norm, u_pointa) * inv_area;
            if (s > 1) { s = 1; }
            if (s < 0) { s = 0; }
            float v = 1 - (orientation(v_norm, u_pointc, u_pointa) * inv_area);
            if (v > 1) { v = 1; }
            if (v < 0) { v = 0; }

            vec3 hsv = vec3(u_angle,
                            s,
                            v);
            color = vec4(hsv_to_rgb(hsv),1);
        }

    }
    if (v_norm.y >= 1)
    {
        int rect_i = int(((v_norm.x+1)/4) * 10) % 5;

        vec3 rect_color = u_colors[rect_i];
        color = vec4(rect_color,1);
        float h = ((v_norm.x+1)/4)*10;
        float epsilon = 0.02;
        float epsilon2 = 0.015;
        if (v_norm.y > 1.4-epsilon ||
            v_norm.y < 1+epsilon ||
            (h <     epsilon && h >   - epsilon) ||
            (h < 1 + epsilon2 && h > 1 - epsilon2) ||
            (h < 2 + epsilon2 && h > 2 - epsilon2) ||
            (h < 3 + epsilon2 && h > 3 - epsilon2) ||
            (h < 4 + epsilon2 && h > 4 - epsilon2) ||
            (h < 5 + epsilon2 && h > 5 - epsilon2) ||
            (h < 6 + epsilon && h > 6 - epsilon))
        {
            color = vec4(0,0,0,1);
        }
    }

    gl_FragColor = color;
}
