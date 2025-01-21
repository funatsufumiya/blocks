#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include "block.h"
#include "helpers.h"

typedef struct
{
    block_t blocks[CHUNK_X][CHUNK_Y][CHUNK_Z];
    SDL_GPUBuffer* opaque_vbo;
    SDL_GPUBuffer* transparent_vbo;
    uint32_t opaque_size;
    uint32_t transparent_size;
    uint32_t opaque_capacity;
    uint32_t transparent_capacity;
    bool skip;
    bool load;
    bool mesh;
}
chunk_t;

block_t chunk_get_block(
    const chunk_t* chunk,
    const int x,
    const int y,
    const int z);
void chunk_set_block(
    chunk_t* chunk,
    const int x,
    const int y,
    const int z,
    const block_t block);
void chunk_wrap(
    int* x,
    int* y,
    int* z);
bool chunk_in(
    const int x,
    const int y,
    const int z);

typedef struct
{
    chunk_t* chunks[WORLD_X][WORLD_Z];
    int x;
    int z;
}
terrain_t;

void terrain_init(
    terrain_t* terrain);
void terrain_free(
    terrain_t* terrain);
chunk_t* terrain_get(
    const terrain_t* terrain,
    const int x,
    const int z);
bool terrain_in(
    const terrain_t* terrain,
    const int x,
    const int z);
bool terrain_border(
    const terrain_t* terrain,
    const int x,
    const int z);
void terrain_neighbors(
    terrain_t* terrain,
    const int x,
    const int z,
    chunk_t* neighbors[DIRECTION_2]);
chunk_t* terrain_get2(
    const terrain_t* terrain,
    const int x,
    const int z);
bool terrain_in2(
    const terrain_t* terrain,
    const int x,
    const int z);
bool terrain_border2(
    const terrain_t* terrain,
    const int x,
    const int z);
void terrain_neighbors2(
    terrain_t* terrain,
    const int x,
    const int z,
    chunk_t* neighbors[DIRECTION_2]);
int* terrain_move(
    terrain_t* terrain,
    const int x,
    const int z,
    int* size);