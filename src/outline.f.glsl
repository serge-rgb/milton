#version 150

in vec2 v_sizes;

uniform int u_radius;

void main()
{
    float r = length(v_sizes);

    const float girth = 2.5;
    const float ring_alpha = 0.5;

    if (r < u_radius &&
        r > u_radius - girth)
    {
        gl_FragColor = vec4(0,0,0,ring_alpha);
    }
    else if (r < u_radius + girth &&
             r >= u_radius)
    {
        gl_FragColor = vec4(1,1,1,ring_alpha);
    }
    else
    {
        discard;
    }
}

