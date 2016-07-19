#version 150

uniform vec2 u_center;
uniform float u_half_width;
uniform float u_radius;

in vec2 v_norm;

#define PI 3.14159

float radians_to_degrees(float r)
{
    return (180 * r) / PI;
}

// http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
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
        vec2 n = v_norm / length(v_norm);
        vec2 v = vec2(1/200.0,0);
        float dotp = dot(n, v);
        float angle = acos(dotp) / (length(n) * length(v));
        /* if (v_norm.y > 0) */
        /* { */
        /*     angle = (2*PI) - angle; */
        /* } */
        vec3 hsv = vec3(radians_to_degrees(angle),1.0,1.0);
        vec3 rgb = hsv2rgb(hsv);
        color = vec4(rgb,1);
    }
    gl_FragColor = color;
}
