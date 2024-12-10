#version 450

#include "helpers.glsl"

layout(location = 0) in vec2 i_uv;
layout(location = 1) in flat vec3 i_normal;
layout(location = 2) in vec4 i_shadow_position;
layout(location = 3) in flat uint i_shadowed;
layout(location = 4) in float i_fog;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D s_atlas;
layout(set = 2, binding = 1) uniform sampler2D s_shadowmap;
layout(set = 3, binding = 0) uniform t_shadow_vector
{
    vec3 u_shadow_vector;
};

void main()
{
    o_color = get_color(
        s_atlas,
        s_shadowmap,
        i_uv,
        i_normal,
        i_shadow_position.xyz / i_shadow_position.w,
        u_shadow_vector,
        bool(i_shadowed),
        i_fog,
        1.0);
}