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
    SDL_GPUTransferBuffer* tbos[CHUNK_MESH_COUNT],
    uint32_t capacities[CHUNK_MESH_COUNT]);
bool voxel_ibo(
    SDL_GPUDevice* device,
    SDL_GPUBuffer** ibo,
    const uint32_t size);