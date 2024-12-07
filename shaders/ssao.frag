#version 450

#include "helpers.glsl"

layout(location = 0) in vec2 i_uv;
layout(location = 0) out float o_ssao;
layout(set = 2, binding = 0) uniform sampler2D s_position;
layout(set = 2, binding = 1) uniform sampler2D s_uv;
layout(set = 2, binding = 2) uniform usampler2D s_voxel;

bool test(
    const uint direction,
    const vec3 position,
    const vec3 neighbor)
{
    switch (direction)
    {
    case 4: return position.y < neighbor.y;
    case 5: return position.y > neighbor.y;
    case 2: return position.x > neighbor.x;
    case 3: return position.x < neighbor.x;
    case 0: return position.z < neighbor.z;
    case 1: return position.z > neighbor.z;
    }
    return false;
}

void main()
{
    const vec2 uv = texture(s_uv, i_uv).xy;
    if (length(uv) == 0)
    {
        discard;
    }
    const uint voxel = texture(s_voxel, i_uv).x;
    const vec4 position = texture(s_position, i_uv);
    const uint direction = get_direction(voxel);
    const vec2 size = 1.0 / textureSize(s_voxel, 0) * (1.0 / position.w) * 75;
    float ssao = 0.0;
    int kernel = 2;
    for (int x = -kernel; x <= kernel; ++x)
    {
        for (int y = -kernel; y <= kernel; ++y)
        {
            const vec2 origin = i_uv + vec2(x, y) * size;
            const vec2 random = origin + vec2(get_random(origin)) * 0.01;
            const uint neighbor_voxel = texture(s_voxel, random).x;
            const uint neighbor_direction = get_direction(neighbor_voxel);
            const vec3 neighbor_position = texture(s_position, random).xyz;
            const vec2 neighbor_uv = texture(s_uv, random).xy;
            if (length(neighbor_uv) == 0 ||
                direction != neighbor_direction ||
                test(neighbor_direction, position.xyz, neighbor_position))
            {
                ssao += 1.0;
            }
        }
    }
    kernel = kernel * 2 + 1;
    kernel = kernel * kernel;
    kernel -= 1;
    o_ssao = 1.0 - (ssao / float(kernel));
}