#version 450

#include "helpers.glsl"

layout(location = 0) in vec3 i_position;
layout(location = 0) out vec4 o_color;

void main()
{
    const float dy = i_position.y;
    const float dx = length(i_position.xz);
    const float pitch = atan(dy, dx);
    o_color = vec4(get_sky(pitch), 1.0);
}