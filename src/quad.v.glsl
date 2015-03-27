#version 330

layout(location = 0) in vec2 position;

out vec2 coord;

void main()
{
    coord = (position + vec2(1,1))/2;
    // direct to clip space. must be in [-1, 1]^2
    gl_Position = vec4(position, 0.0, 1.0);
}

