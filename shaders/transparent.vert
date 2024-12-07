#version 450

#include "helpers.glsl"

layout(location = 0) in uint i_voxel;
layout(location = 0) out vec2 o_uv;
layout(location = 1) out flat vec3 o_normal;
layout(location = 2) out vec4 o_shadow_position;
layout(location = 3) out flat uint o_shadowed;
layout(location = 4) out float o_fog;
layout(set = 1, binding = 0) uniform t_position
{
    ivec3 u_position;
};
layout(set = 1, binding = 1) uniform t_matrix
{
    mat4 u_matrix;
};
layout(set = 1, binding = 2) uniform t_player_position
{
    vec3 u_player_position;
};
layout(set = 1, binding = 3) uniform t_shadow_matrix
{
    mat4 u_shadow_matrix;
};

void main()
{
    vec3 position = u_position + get_position(i_voxel);
    o_uv = get_uv(i_voxel);
    o_shadowed = uint(get_shadowed(i_voxel));
    o_fog = get_fog(distance(position.xz, u_player_position.xz));
    gl_Position = u_matrix * vec4(position, 1.0);
    if (!bool(o_shadowed))
    {
        return;
    }
    o_shadow_position = bias * u_shadow_matrix * vec4(position, 1.0);
    o_normal = get_normal(i_voxel);
}