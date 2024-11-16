#ifndef HELPERS_GLSL
#define HELPERS_GLSL

#include "config.h"

const vec3 normals[6] = vec3[6]
(
    vec3( 0, 0, 1),
    vec3( 0, 0,-1),
    vec3( 1, 0, 0),
    vec3(-1, 0, 0),
    vec3( 0, 1, 0),
    vec3( 0,-1, 0)
);

const mat4 bias = mat4
(
    0.5,
    0.0,
    0.0,
    0.0,
    0.0,
   -0.5,
    0.0,
    0.0,
    0.0,
    0.0,
    1.0,
    0.0,
    0.5,
    0.5,
    0.0,
    1.0
);

vec3 get_position(const uint voxel)
{
    return vec3(
        voxel >> VOXEL_X_OFFSET & VOXEL_X_MASK,
        voxel >> VOXEL_Y_OFFSET & VOXEL_Y_MASK,
        voxel >> VOXEL_Z_OFFSET & VOXEL_Z_MASK);
}

vec2 get_atlas(const vec2 position)
{
    vec2 uv;
    uv.x = position.x / ATLAS_WIDTH * ATLAS_FACE_WIDTH;
    uv.y = position.y / ATLAS_HEIGHT * ATLAS_FACE_HEIGHT;
    return uv;
}

vec2 get_uv(const uint voxel)
{
    const vec2 uv = vec2(
        voxel >> VOXEL_U_OFFSET & VOXEL_U_MASK,
        voxel >> VOXEL_V_OFFSET & VOXEL_V_MASK);
    return get_atlas(uv);
}

uint get_direction(const uint voxel)
{
    return voxel >> VOXEL_DIRECTION_OFFSET & VOXEL_DIRECTION_MASK;
}

vec3 get_normal(const uint voxel)
{
    return normals[get_direction(voxel)];
}

bool get_shadow(const uint voxel)
{
    return bool(voxel >> VOXEL_SHADOW_OFFSET & VOXEL_SHADOW_MASK);
}

vec3 get_sky(const float y)
{
    return mix(vec3(0.3, 0.6, 0.9), vec3(0.8, 0.95, 1.0), max(y - 0.5, 0.0));
}

float get_fog(const vec2 position, const vec2 camera)
{
    return pow(clamp(length(position - camera) / 250.0, 0.0, 1.0), 2.5);
}

float get_random(const vec2 position)
{
    return fract(sin(dot(position, vec2(12.9898, 78.233))) * 43758.5453);
}

float get_light(const vec3 normal, const vec3 camera)
{
    return max(dot(normal, -camera), 0.0);
}

bool get_shadowed(
    const vec3 normal,
    const vec3 camera,
    const vec3 position,
    const sampler2D map)
{
    return
        (dot(normal, -camera) < 0.0) || (
        all(greaterThanEqual(position, vec3(0.0))) &&
        all(lessThanEqual(position, vec3(1.0))) &&
        (position.z - 0.0005 > texture(map, position.xy).x));
}

bool get_edge(
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

vec4 get_color(
    const vec4 color,
    const bool shadowed,
    const float ssao,
    const float fog,
    const vec3 normal,
    const vec3 camera)
{
    float a;
    float b;
    float c;
    const float light = get_light(normal, camera);
#if DRAW_NORMALS
    return vec4(vec3(normal * 0.5 + 0.5), 1.0);
#endif
#if DRAW_SSAO
    return vec4(vec3(ssao), 1.0);
#endif
#if DRAW_LIGHTS
    return vec4(vec3(light), 1.0);
#endif
#if DRAW_COLORS
    return vec4(color);
#endif
#if DRAW_SHADOWED
    return vec4(vec3(!shadowed), 1.0);
#endif
    if (shadowed)
    {
        a = ssao * 0.2;
        b = 0.0;
        c = 0.0;
    }
    else
    {
        a = ssao * 0.3;
        b = 0.4;
        c = light * 0.6;
    }
    const vec4 composite = vec4(color.xyz * (a + b + c + 0.3), color.a);
    const vec4 sky = vec4(get_sky(0.0), 1.0);
    return mix(composite, sky, fog);
}

#endif