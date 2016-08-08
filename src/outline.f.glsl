#version 150

in vec2 v_sizes;

uniform int u_radius;
uniform bool u_fill;
uniform vec4 u_color;

void main()
{
    float r = length(v_sizes);

    float girth = u_fill ? 4.0 : 2.0;
    const float ring_alpha = 0.4;

    if (r <= u_radius &&
        r > u_radius - girth)
    {
        gl_FragColor = vec4(0,0,0,ring_alpha);
    }
    else if (r < u_radius + girth && r >= u_radius)
    {
        gl_FragColor = vec4(1,1,1,ring_alpha);
    }
    else if (u_fill && r < u_radius)
    {
        gl_FragColor = u_color;
    }
    else
    {
        discard;
    }
}

