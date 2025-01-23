#version 450

layout(location = 0) in vec2 i_uv;
layout(location = 0) out float o_random;

void main()
{
    o_random = fract(sin(dot(i_uv, vec2(12.9898, 78.233))) * 43758.5453);
}