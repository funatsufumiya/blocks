#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include "chunk.h"
#include "helpers.h"

bool voxel_vbo(
    chunk_t* chunk,
    const chunk_t* neighbors[DIRECTION_2],
    SDL_GPUDevice* device,
    SDL_GPUTransferBuffer** opaque_tbo,
    SDL_GPUTransferBuffer** transparent_tbo,
    uint32_t* opaque_capacity,
    uint32_t* transparent_capacity);
bool voxel_ibo(
    SDL_GPUDevice* device,
    SDL_GPUBuffer** ibo,
    const uint32_t size);