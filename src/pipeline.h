#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef enum
{
    PIPELINE_SKY,
    PIPELINE_SHADOW,
    PIPELINE_OPAQUE,
    PIPELINE_SSAO,
    PIPELINE_COMPOSITE,
    PIPELINE_TRANSPARENT,
    PIPELINE_RAYCAST,
    PIPELINE_UI,
    PIPELINE_RANDOM,
    PIPELINE_COUNT
}
pipeline_t;

bool pipeline_init(
    SDL_GPUDevice* device,
    const SDL_GPUTextureFormat format);
void pipeline_free();
void pipeline_bind(
    void* pass,
    const pipeline_t pipeline);