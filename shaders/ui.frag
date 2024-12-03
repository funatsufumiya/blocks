#version 450

#include "helpers.glsl"

layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D s_atlas;
layout(set = 3, binding = 0) uniform t_viewport
{
    ivec2 u_viewport;
};
layout(set = 3, binding = 1) uniform t_block
{
    ivec2 u_block;
};

void main()
{
    const vec2 position = vec2(gl_FragCoord.x, u_viewport.y - gl_FragCoord.y);
    const vec2 center = u_viewport / 2.0;
    const vec2 ratio = vec2(u_viewport) / vec2(APP_WIDTH, APP_HEIGHT);
    const float scale = min(ratio.x, ratio.y);
    const float block_width = 50 * scale;
    const float block_start = 10 * scale;
    const vec2 block_end = vec2(block_start + block_width);
    if (position.x > block_start && position.x < block_end.x &&
        position.y > block_start && position.y < block_end.y)
    {
        const float x = (position.x - block_start) / block_width;
        const float y = (position.y - block_start) / block_width;
        const vec2 uv = get_atlas(u_block);
        const float c = uv.x + x / ATLAS_X_FACES;
        const float d = uv.y + (1.0 - y) / ATLAS_Y_FACES;
        o_color = texture(s_atlas, vec2(c, d));
        o_color.xyz *= 1.25;
        return;
    }
    const float cross_width = 8 * scale;
    const float cross_thickness = 2 * scale;
    const vec2 cross_start1 = center - vec2(cross_width, cross_thickness);
    const vec2 cross_end1 = center + vec2(cross_width, cross_thickness);
    const vec2 cross_start2 = center - vec2(cross_thickness, cross_width);
    const vec2 cross_end2 = center + vec2(cross_thickness, cross_width);
    if ((position.x > cross_start1.x && position.y > cross_start1.y &&
        position.x < cross_end1.x && position.y < cross_end1.y) ||
        (position.x > cross_start2.x && position.y > cross_start2.y &&
        position.x < cross_end2.x && position.y < cross_end2.y))
    {
        o_color = vec4(1.0);
        return;
    }
    discard;
}