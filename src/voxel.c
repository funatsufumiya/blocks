#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include "block.h"
#include "helpers.h"
#include "voxel.h"
#include "world.h"

static uint32_t pack(
    const block_t block,
    const int x,
    const int y,
    const int z,
    const int u,
    const int v,
    const direction_t direction)
{
    static_assert(VOXEL_X_OFFSET + VOXEL_X_BITS <= 32, "");
    static_assert(VOXEL_Y_OFFSET + VOXEL_Y_BITS <= 32, "");
    static_assert(VOXEL_Z_OFFSET + VOXEL_Z_BITS <= 32, "");
    static_assert(VOXEL_U_OFFSET + VOXEL_U_BITS <= 32, "");
    static_assert(VOXEL_V_OFFSET + VOXEL_V_BITS <= 32, "");
    static_assert(VOXEL_DIRECTION_OFFSET + VOXEL_DIRECTION_BITS <= 32, "");
    static_assert(VOXEL_SHADOW_OFFSET + VOXEL_SHADOW_BITS <= 32, "");
    static_assert(VOXEL_SHADOWED_OFFSET + VOXEL_SHADOWED_BITS <= 32, "");
    assert(x <= VOXEL_X_MASK);
    assert(y <= VOXEL_Y_MASK);
    assert(z <= VOXEL_Z_MASK);
    assert(u <= VOXEL_U_MASK);
    assert(v <= VOXEL_V_MASK);
    assert(direction <= VOXEL_DIRECTION_MASK);
    uint32_t voxel = 0;
    voxel |= x << VOXEL_X_OFFSET;
    voxel |= y << VOXEL_Y_OFFSET;
    voxel |= z << VOXEL_Z_OFFSET;
    voxel |= u << VOXEL_U_OFFSET;
    voxel |= v << VOXEL_V_OFFSET;
    voxel |= direction << VOXEL_DIRECTION_OFFSET;
    voxel |= block_shadow(block) << VOXEL_SHADOW_OFFSET;
    voxel |= block_shadowed(block) << VOXEL_SHADOWED_OFFSET;
    return voxel;
}

static uint32_t pack_non_sprite(
    const block_t block,
    const int x,
    const int y,
    const int z,
    const direction_t direction,
    const int i)
{
    assert(block > BLOCK_EMPTY);
    assert(block < BLOCK_COUNT);
    assert(direction < DIRECTION_3);
    assert(i < 4);
    static const int positions[][4][3] =
    {
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}},
        {{1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}},
        {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 1, 1}},
        {{0, 1, 0}, {1, 1, 0}, {0, 1, 1}, {1, 1, 1}},
        {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {1, 0, 1}},
    };
    static const int uvs[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
    };
    const int a = positions[direction][i][0] + x;
    const int b = positions[direction][i][1] + y;
    const int c = positions[direction][i][2] + z;
    const int d = uvs[direction][i][0] + blocks[block][direction][0];
    const int e = uvs[direction][i][1] + blocks[block][direction][1];
    return pack(block, a, b, c, d, e, direction);
}

static uint32_t pack_sprite(
    const block_t block,
    const int x,
    const int y,
    const int z,
    const int direction,
    const int i)
{
    assert(block > BLOCK_EMPTY);
    assert(block < BLOCK_COUNT);
    assert(direction < 4);
    assert(i < 4);
    static const int positions[][4][3] =
    {
        {{0, 0, 0}, {0, 1, 0}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 1}, {0, 1, 0}, {1, 1, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 1}, {1, 1, 0}},
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 0}, {1, 1, 0}},
    };
    static const int uvs[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
    };
    const int a = positions[direction][i][0] + x;
    const int b = positions[direction][i][1] + y;
    const int c = positions[direction][i][2] + z;
    const int d = uvs[direction][i][0] + blocks[block][DIRECTION_N][0];
    const int e = uvs[direction][i][1] + blocks[block][DIRECTION_N][1];
    return pack(block, a, b, c, d, e, DIRECTION_U);
}

static void fill(
    const chunk_t* chunk,
    const chunk_t* neighbors[DIRECTION_2],
    uint32_t* datas[CHUNK_MESH_COUNT],
    uint32_t sizes[CHUNK_MESH_COUNT],
    const uint32_t capacities[CHUNK_MESH_COUNT])
{
    assert(chunk);
    for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
    {
        sizes[mesh] = 0;
    }
    for (int x = 0; x < CHUNK_X; x++)
    for (int y = 0; y < CHUNK_Y; y++)
    for (int z = 0; z < CHUNK_Z; z++)
    {
        const block_t a = chunk->blocks[x][y][z];
        if (a == BLOCK_EMPTY)
        {
            continue;
        }
        chunk_mesh_t mesh;
        if (block_opaque(a))
        {
            mesh = CHUNK_MESH_OPAQUE;
        }
        else
        {
            mesh = CHUNK_MESH_TRANSPARENT;
        }
        if (block_sprite(a))
        {
            sizes[mesh] += 4;
            if (sizes[mesh] > capacities[mesh])
            {
                continue;
            }
            for (int direction = 0; direction < 4; direction++)
            {
                for (int i = 0; i < 4; i++)
                {
                    const int j = sizes[mesh] * 4 - 4 * (direction + 1) + i;
                    datas[mesh][j] = pack_sprite(a, x, y, z, direction, i);
                }            
            }
            continue;
        }
        for (direction_t d = 0; d < DIRECTION_3; d++)
        {
            if (y == 0 && d != DIRECTION_U)
            {
                continue;
            }
            block_t b;
            int s = x + directions[d][0];
            int t = y + directions[d][1];
            int p = z + directions[d][2];
            if (chunk_in(s, t, p))
            {
                b = chunk->blocks[s][t][p];
            }
            else if (d < DIRECTION_2 && neighbors[d])
            {
                chunk_wrap(&s, &t, &p);
                b = neighbors[d]->blocks[s][t][p];
            }
            else
            {
                b = BLOCK_EMPTY;
            }
            if (b != BLOCK_EMPTY && !block_sprite(b) && !(block_opaque(a) && !block_opaque(b)))
            {
                continue;
            }
            if (++sizes[mesh] > capacities[mesh])
            {
                continue;
            }
            for (int i = 0; i < 4; i++)
            {
                const int j = sizes[mesh] * 4 - 4 + i;
                datas[mesh][j] = pack_non_sprite(a, x, y, z, d, i);    
            }
        }
    }
}

bool voxel_vbo(
    chunk_t* chunk,
    const chunk_t* neighbors[DIRECTION_2],
    SDL_GPUDevice* device,
    SDL_GPUTransferBuffer* tbos[CHUNK_MESH_COUNT],
    uint32_t capacities[CHUNK_MESH_COUNT])
{
    assert(chunk);
    assert(device);
    uint32_t* datas[CHUNK_MESH_COUNT] = {0};
    for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
    {
        if (!tbos[mesh])
        {
            continue;
        }
        datas[mesh] = SDL_MapGPUTransferBuffer(device, tbos[mesh], true);
        if (!datas[mesh])
        {
            SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
            return false;
        }
    }
    fill(
        chunk,
        neighbors,
        datas,
        chunk->sizes,
        capacities);
    for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
    {
        if (datas[mesh])
        {
            SDL_UnmapGPUTransferBuffer(device, tbos[mesh]);
            datas[mesh] = NULL;
        }
    }
    bool status = false;
    for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
    {
        if (chunk->sizes[mesh])
        {
            status = true;
            break;
        }
    }
    if (!status)
    {
        return true;
    }
    status = false;
    for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
    {
        if (chunk->sizes[mesh] > capacities[mesh])
        {
            status = true;
            break;
        }
    }
    if (status)
    {
        for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
        {
            if (chunk->sizes[mesh] <= capacities[mesh])
            {
                continue;
            }
            if (tbos[mesh])
            {
                SDL_ReleaseGPUTransferBuffer(device, tbos[mesh]);
                tbos[mesh] = NULL;
                capacities[mesh] = 0;
            }
            SDL_GPUTransferBufferCreateInfo tbci = {0};
            tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            tbci.size = chunk->sizes[mesh] * 16;
            tbos[mesh] = SDL_CreateGPUTransferBuffer(device, &tbci);
            if (!tbos[mesh])
            {
                SDL_Log("Failed to create tbo buffer: %s", SDL_GetError());
                return false;
            }
            capacities[mesh] = chunk->sizes[mesh];
        }
        for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
        {
            if (!chunk->sizes[mesh])
            {
                continue;
            }
            datas[mesh] = SDL_MapGPUTransferBuffer(device, tbos[mesh], true);
            if (!datas[mesh])
            {
                SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
                return false;
            }
        }
        fill(
            chunk,
            neighbors,
            datas,
            chunk->sizes,
            capacities);
        for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
        {
            if (datas[mesh])
            {
                SDL_UnmapGPUTransferBuffer(device, tbos[mesh]);
                datas[mesh] = NULL;
            }
        }
    }
    for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
    {
        if (chunk->sizes[mesh] <= chunk->capacities[mesh])
        {
            continue;
        }
        if (chunk->vbos[mesh])
        {
            SDL_ReleaseGPUBuffer(device, chunk->vbos[mesh]);
            chunk->vbos[mesh] = NULL;
            chunk->capacities[mesh] = 0;
        }
        SDL_GPUBufferCreateInfo bci = {0};
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = chunk->sizes[mesh] * 16;
        chunk->vbos[mesh]= SDL_CreateGPUBuffer(device, &bci);
        if (!chunk->vbos[mesh])
        {
            SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
            return false;
        }
        chunk->capacities[mesh] = chunk->sizes[mesh];
    }
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTransferBufferLocation location = {0};
    SDL_GPUBufferRegion region = {0};
    for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
    {
        if (!chunk->sizes[mesh])
        {
            continue;
        }
        location.transfer_buffer = tbos[mesh];
        region.size = chunk->sizes[mesh] * 16;
        region.buffer = chunk->vbos[mesh];
        SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    }
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);
    return true;
}

bool voxel_ibo(
    SDL_GPUDevice* device,
    SDL_GPUBuffer** ibo,
    const uint32_t size)
{
    assert(device);
    assert(ibo);
    assert(size);
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = size * 24;
    SDL_GPUTransferBuffer* tbo = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!tbo)
    {
        SDL_Log("Failed to create tbo buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUBufferCreateInfo bci = {0};
    bci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    bci.size = size * 24;
    *ibo = SDL_CreateGPUBuffer(device, &bci);
    if (!(*ibo))
    {
        SDL_Log("Failed to create index buffer: %s", SDL_GetError());
        return false;
    }
    uint32_t* data = SDL_MapGPUTransferBuffer(device, tbo, false);
    if (!data)
    {
        SDL_Log("Failed to map tbo buffer: %s", SDL_GetError());
        return false;
    }
    for (uint32_t i = 0; i < size; i++)
    {
        data[i * 6 + 0] = i * 4 + 0;
        data[i * 6 + 1] = i * 4 + 1;
        data[i * 6 + 2] = i * 4 + 2;
        data[i * 6 + 3] = i * 4 + 3;
        data[i * 6 + 4] = i * 4 + 2;
        data[i * 6 + 5] = i * 4 + 1;
    }
    SDL_UnmapGPUTransferBuffer(device, tbo);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTransferBufferLocation location = {0};
    location.transfer_buffer = tbo;
    SDL_GPUBufferRegion region = {0};
    region.size = size * 24;
    region.buffer = *ibo;
    SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device, tbo);
    return true;
}