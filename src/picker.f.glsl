#version 150

uniform vec2 u_center;
uniform float u_half_width;
uniform float u_radius;

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
void main()
{
    // NOTE:
    //  These constants gotten from gui.cc in gui_init. From bounds_radius_px, wheel_half_width, and so on.
    float half_width = 12.0 / 100.0;
    float radius = 1.0 - (12.0 + 5) / 100.0;


    vec4 color = vec4(1,0,0,1);
    if (v_norm.y <= 1)
    {
        color = vec4(0,0,1,1);
    }
    float dist = distance(vec2(0), v_norm);
    if (dist < radius+half_width &&
        dist > radius-half_width)
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
    gl_FragColor = color;
}
