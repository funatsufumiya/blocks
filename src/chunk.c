#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "chunk.h"
#include "helpers.h"

block_t chunk_get_block(
    const chunk_t* chunk,
    const int x,
    const int y,
    const int z)
{
    assert(chunk);
    assert(chunk_in(x, y, z));
    assert(!chunk->load);
    return chunk->blocks[x][y][z];
}

void chunk_set_block(
    chunk_t* chunk,
    const int x,
    const int y,
    const int z,
    const block_t block)
{
    assert(chunk);
    assert(chunk_in(x, y, z));
    chunk->blocks[x][y][z] = block;
    chunk->skip = false;
}

void chunk_wrap(
    int* x,
    int* y,
    int* z)
{
    assert(x);
    assert(y);
    assert(z);
    *x = (*x % CHUNK_X + CHUNK_X) % CHUNK_X;
    *y = (*y % CHUNK_Y + CHUNK_Y) % CHUNK_Y;
    *z = (*z % CHUNK_Z + CHUNK_Z) % CHUNK_Z;
}

bool chunk_in(
    const int x,
    const int y,
    const int z)
{
    return
        x >= 0 &&
        y >= 0 &&
        z >= 0 &&
        x < CHUNK_X &&
        y < CHUNK_Y &&
        z < CHUNK_Z;
}

void terrain_init(
    terrain_t* terrain)
{
    assert(terrain);
    terrain->x = INT_MAX;
    terrain->z = INT_MAX;
    for (int x = 0; x < WORLD_X; x++)
    for (int z = 0; z < WORLD_Z; z++)
    {
        terrain->chunks[x][z] = calloc(1, sizeof(chunk_t));
        assert(terrain->chunks[x][z]);
    }
}

void terrain_free(
    terrain_t* terrain)
{
    assert(terrain);
    for (int x = 0; x < WORLD_X; x++)
    for (int z = 0; z < WORLD_Z; z++)
    {
        free(terrain->chunks[x][z]);
        terrain->chunks[x][z] = NULL;
    }
}

chunk_t* terrain_get(
    const terrain_t* terrain,
    const int x,
    const int z)
{
    assert(terrain);
    assert(terrain_in(terrain, x, z));
    assert(terrain->chunks[x][z]);
    return terrain->chunks[x][z];
}

bool terrain_in(
    const terrain_t* terrain,
    const int x,
    const int z)
{
    assert(terrain);
    return
        x >= 0 &&
        z >= 0 &&
        x < WORLD_X &&
        z < WORLD_Z;
}

bool terrain_border(
    const terrain_t* terrain,
    const int x,
    const int z)
{
    assert(terrain);
    return
        x == 0 ||
        z == 0 ||
        x == WORLD_X - 1 ||
        z == WORLD_Z - 1;
}

void terrain_neighbors(
    terrain_t* terrain,
    const int x,
    const int z,
    chunk_t* neighbors[DIRECTION_2])
{
    assert(terrain);
    assert(terrain_in(terrain, x, z));
    for (direction_t d = 0; d < DIRECTION_2; d++)
    {
        const int a = x + directions[d][0];
        const int b = z + directions[d][2];
        if (terrain_in(terrain, a, b))
        {
            neighbors[d] = terrain_get(terrain, a, b);
        }
        else
        {
            neighbors[d] = NULL;
        }
    }
}

chunk_t* terrain_get2(
    const terrain_t* terrain,
    int x,
    int z)
{
    assert(terrain);
    x -= terrain->x;
    z -= terrain->z;
    return terrain_get(terrain, x, z);
}

bool terrain_in2(
    const terrain_t* terrain,
    int x,
    int z)
{
    assert(terrain);
    x -= terrain->x;
    z -= terrain->z;
    return terrain_in(terrain, x, z);
}

bool terrain_border2(
    const terrain_t* terrain,
    int x,
    int z)
{
    assert(terrain);
    x -= terrain->x;
    z -= terrain->z;
    return terrain_border(terrain, x, z);
}

void terrain_neighbors2(
    terrain_t* terrain,
    int x,
    int z,
    chunk_t* neighbors[DIRECTION_2])
{
    assert(terrain);
    x -= terrain->x;
    z -= terrain->z;
    terrain_neighbors(terrain, x, z, neighbors);
}

int* terrain_move(
    terrain_t* terrain,
    const int x,
    const int z,
    int* size)
{
    assert(terrain);
    assert(size);
    *size = 0;
    const int a = x - terrain->x;
    const int b = z - terrain->z;
    if (!a && !b)
    {
        return NULL;
    }
    terrain->x = x;
    terrain->z = z;
    chunk_t* in[WORLD_X][WORLD_Z] = {0};
    chunk_t* out[WORLD_CHUNKS];
    int* indices = malloc(WORLD_CHUNKS * 2 * sizeof(int));
    assert(indices);
    for (int i = 0; i < WORLD_X; i++)
    for (int j = 0; j < WORLD_Z; j++)
    {
        const int c = i - a;
        const int d = j - b;
        if (terrain_in(terrain, c, d))
        {
            in[c][d] = terrain_get(terrain, i, j);
        }
        else
        {
            out[(*size)++] = terrain_get(terrain, i, j);
        }
        terrain->chunks[i][j] = NULL;
    }
    memcpy(terrain->chunks, in, sizeof(in));
    int n = *size;
    for (int i = 0; i < WORLD_X; i++)
    for (int j = 0; j < WORLD_Z; j++)
    {
        if (terrain->chunks[i][j])
        {
            continue;
        }
        --n;
        terrain->chunks[i][j] = out[n];
        indices[n * 2 + 0] = i;
        indices[n * 2 + 1] = j;
    }
    assert(!n);
    return indices;
}