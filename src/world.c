#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include "block.h"
#include "camera.h"
#include "containers.h"
#include "database.h"
#include "helpers.h"
#include "noise.h"
#include "voxmesh.h"
#include "world.h"

typedef enum
{
    JOB_TYPE_QUIT,
    JOB_TYPE_LOAD,
    JOB_TYPE_MESH,
}
job_type_t;

typedef struct
{
    job_type_t type;
    int x;
    int y;
    int z;
}
job_t;

typedef struct
{
    thrd_t thrd;
    mtx_t mtx;
    cnd_t cnd;
    const job_t* job;
    SDL_GPUTransferBuffer* opaque_tbo;
    SDL_GPUTransferBuffer* transparent_tbo;
    uint32_t opaque_size;
    uint32_t transparent_size;
}
worker_t;

static terrain_t terrain;
static SDL_GPUDevice* device;
static SDL_GPUBuffer* ibo;
static uint32_t ibo_size;
static worker_t workers[WORLD_WORKERS];
static int sorted2d[WORLD_GROUPS][2];
static int sorted3d[WORLD_Y][WORLD_CHUNKS][3];
static int height;

static void get_neighbors2(
    const int x,
    const int y,
    const int z,
    chunk_t* neighbors[DIRECTION_3])
{
    for (direction_t d = 0; d < DIRECTION_3; d++)
    {
        const int a = x + directions[d][0];
        const int b = y + directions[d][1];
        const int c = z + directions[d][2];
        if (terrain_in2(&terrain, a, c) && b >= 0 && b < WORLD_Y)
        {
            neighbors[d] = &terrain_get2(&terrain, a, c)->chunks[b];
        }
        else
        {
            neighbors[d] = NULL;
        }
    }
}

static int loop(void* args)
{
    assert(args);
    worker_t* worker = args;
    while (true)
    {
        mtx_lock(&worker->mtx);
        while (!worker->job)
        {
            cnd_wait(&worker->cnd, &worker->mtx);
        }
        if (worker->job->type == JOB_TYPE_QUIT)
        {
            worker->job = NULL;
            cnd_signal(&worker->cnd);
            mtx_unlock(&worker->mtx);
            return 0;
        }
        const int x = terrain.x + worker->job->x;
        const int y = worker->job->y;
        const int z = terrain.z + worker->job->z;
        group_t* group = terrain_get2(&terrain, x, z);
        switch (worker->job->type)
        {
        case JOB_TYPE_LOAD:
            assert(group->dirty);
            noise_generate(group, x, z);
            database_get_blocks(group, x, z);
            group->dirty = false;
            break;
        case JOB_TYPE_MESH:
            chunk_t* chunk = &group->chunks[y];
            assert(!chunk->empty);
            chunk_t* neighbors[DIRECTION_3];
            get_neighbors2(x, y, z, neighbors);
            if (voxmesh_vbo(
                chunk,
                neighbors,
                y,
                device,
                &worker->opaque_tbo,
                &worker->transparent_tbo,
                &worker->opaque_size,
                &worker->transparent_size))
            {
                chunk->dirty = false;
            }
            break;
        default:
            assert(0);
        }
        worker->job = NULL;
        cnd_signal(&worker->cnd);
        mtx_unlock(&worker->mtx);
    }
    return 0;
}

static void dispatch(worker_t* worker, const job_t* job)
{
    assert(worker);
    assert(job);
    mtx_lock(&worker->mtx);
    assert(!worker->job);
    worker->job = job;
    cnd_signal(&worker->cnd);
    mtx_unlock(&worker->mtx);
}

static void wait(worker_t* worker)
{
    assert(worker);
    mtx_lock(&worker->mtx);
    while (worker->job)
    {
        cnd_wait(&worker->cnd, &worker->mtx);
    }
    mtx_unlock(&worker->mtx);
}

bool world_init(SDL_GPUDevice* handle)
{
    assert(handle);
    device = handle;
    height = INT32_MAX;
    terrain_init(&terrain);
    for (int i = 0; i < WORLD_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        memset(worker, 0, sizeof(worker_t));
        if (mtx_init(&worker->mtx, mtx_plain) != thrd_success)
        {
            SDL_Log("Failed to create mutex");
            return false;
        }
        if (cnd_init(&worker->cnd) != thrd_success)
        {
            SDL_Log("Failed to create condition variable");
            return false;
        }
        if (thrd_create(&worker->thrd, loop, worker) != thrd_success)
        {
            SDL_Log("Failed to create thread");
            return false;
        }
    }
    for (int i = 0; i < WORLD_Y; i++)
    {
        int j = 0;
        for (int x = 0; x < WORLD_X; x++)
        for (int y = 0; y < WORLD_Y; y++)
        for (int z = 0; z < WORLD_Z; z++)
        {
            sorted3d[i][j][0] = x;
            sorted3d[i][j][1] = y;
            sorted3d[i][j][2] = z;
            j++;
        }
        const int w = WORLD_X / 2;
        const int h = WORLD_Z / 2;
        sort_3d(w, i, h, sorted3d[i], WORLD_CHUNKS);
    }
    int j = 0;
    for (int x = 0; x < WORLD_X; x++)
    for (int z = 0; z < WORLD_Z; z++)
    {
        sorted2d[j][0] = x;
        sorted2d[j][1] = z;
        j++;
    }
    const int w = WORLD_X / 2;
    const int h = WORLD_Z / 2;
    sort_2d(w, h, sorted2d, WORLD_GROUPS);
    return true;
}

void world_free()
{
    job_t job;
    job.type = JOB_TYPE_QUIT;
    for (int i = 0; i < WORLD_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        job.type = JOB_TYPE_QUIT;
        dispatch(worker, &job);
    }
    for (int i = 0; i < WORLD_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        thrd_join(worker->thrd, NULL);
        mtx_destroy(&worker->mtx);
        cnd_destroy(&worker->cnd);
        if (worker->opaque_tbo)
        {
            SDL_ReleaseGPUTransferBuffer(device, worker->opaque_tbo);
            worker->opaque_tbo = NULL;
        }
        if (worker->transparent_tbo)
        {
            SDL_ReleaseGPUTransferBuffer(device, worker->transparent_tbo);
            worker->transparent_tbo = NULL;
        }
    }
    for (int x = 0; x < WORLD_X; x++)
    {
        for (int z = 0; z < WORLD_Z; z++)
        {
            group_t* group = terrain_get(&terrain, x, z);
            for (int i = 0; i < GROUP_CHUNKS; i++)
            {
                chunk_t* chunk = &group->chunks[i];
                if (chunk->opaque_vbo)
                {
                    SDL_ReleaseGPUBuffer(device, chunk->opaque_vbo);
                    chunk->opaque_vbo = NULL;
                }
                if (chunk->transparent_vbo)
                {
                    SDL_ReleaseGPUBuffer(device, chunk->transparent_vbo);
                    chunk->transparent_vbo = NULL;
                }
            }
        }
    }
    terrain_free(&terrain);
    if (ibo)
    {
        SDL_ReleaseGPUBuffer(device, ibo);
        ibo = NULL;
    }
    device = NULL;
    ibo_size = 0;
}

static void move(
    const int x,
    const int y,
    const int z)
{
    const int a = x / CHUNK_X - WORLD_X / 2;
    const int b = y / CHUNK_Y;
    const int c = z / CHUNK_Z - WORLD_Z / 2;
    height = clamp(b, 0, WORLD_Y - 1);
    int size;
    int* data = terrain_move(&terrain, a, c, &size);
    if (!data)
    {
        return;
    }
    for (int i = 0; i < size; i++)
    {
        const int j = data[i * 2 + 0];
        const int k = data[i * 2 + 1];
        group_t* group = terrain_get(&terrain, j, k);
        for (int j = 0; j < GROUP_CHUNKS; j++)
        {
            chunk_t* chunk = &group->chunks[j];
            memset(chunk->blocks, 0, sizeof(chunk->blocks));
            chunk->dirty = true;
            chunk->empty = true;
        }
        group->dirty = true;
    }
    free(data);
}

void world_update(
    const int x,
    const int y,
    const int z)
{
    move(x, y, z);
    int n = 0;
    job_t jobs[WORLD_WORKERS];
    for (int i = 0; i < WORLD_GROUPS && n < WORLD_WORKERS; i++)
    {
        const int j = sorted2d[i][0];
        const int k = sorted2d[i][1];
        group_t* group = terrain_get(&terrain, j, k);
        if (group->dirty)
        {
            job_t* job = &jobs[n++];
            job->type = JOB_TYPE_LOAD;
            job->x = j;
            job->y = 0;
            job->z = k;
            continue;
        }
        if (terrain_border(&terrain, j, k))
        {
            continue;
        }
        bool dirty = false;
        group_t* neighbors[DIRECTION_2];
        terrain_neighbors(&terrain, j, k, neighbors);
        for (direction_t direction = 0; direction < DIRECTION_2; direction++)
        {
            const group_t* neighbor = neighbors[direction];
            if (!neighbor || neighbor->dirty)
            {
                dirty = true;
                break;
            }
        }
        if (dirty)
        {
            continue;
        }
        for (int h = 0; h < GROUP_CHUNKS && n < WORLD_WORKERS; h++)
        {
            chunk_t* chunk = &group->chunks[h];
            if (!chunk->dirty || chunk->empty)
            {
                continue;
            }
            job_t* job = &jobs[n++];
            job->type = JOB_TYPE_MESH;
            job->x = j;
            job->y = h;
            job->z = k;
        }
    }
    uint32_t size = 0;
    for (int i = 0; i < n; i++)
    {
        dispatch(&workers[i], &jobs[i]);
    }
    for (int i = 0; i < n; i++)
    {
        wait(&workers[i]);
    }
    for (int i = 0; i < n; i++)
    {
        const job_t* job = &jobs[i];
        if (job->type != JOB_TYPE_MESH)
        {
            continue;
        }
        group_t* group = terrain_get(&terrain, job->x, job->z);
        size = max3(
            size,
            group->chunks[job->y].opaque_size,
            group->chunks[job->y].transparent_size);
    }
    if (size > ibo_size)
    {
        if (ibo)
        {
            SDL_ReleaseGPUBuffer(device, ibo);
            ibo = NULL;
            ibo_size = 0;
        }
        if (voxmesh_ibo(device, &ibo, size))
        {
            ibo_size = size;
        }
    }
}

void world_render(
    const camera_t* camera,
    SDL_GPUCommandBuffer* commands,
    SDL_GPURenderPass* pass,
    const world_pass_type_t type)
{
    assert(commands);
    assert(pass);
    if (!ibo)
    {
        return;
    }
    SDL_GPUBufferBinding ibb = {0};
    ibb.buffer = ibo;
    SDL_BindGPUIndexBuffer(pass, &ibb, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    for (int i = 0; i < WORLD_CHUNKS; i++)
    {
        int j;
        if (type == WORLD_PASS_TYPE_OPAQUE)
        {
            j = i;
        }
        else
        {
            j = WORLD_CHUNKS - i - 1;
        }
        int x = sorted3d[height][j][0] + terrain.x;
        int y = sorted3d[height][j][1];
        int z = sorted3d[height][j][2] + terrain.z;
        if (terrain_border2(&terrain, x, z))
        {
            continue;
        }
        const group_t* group = terrain_get2(&terrain, x, z);
        if (group->dirty)
        {
            continue;
        }
        const chunk_t* chunk = &group->chunks[y];
        if (chunk->dirty)
        {
            continue;
        }
        SDL_GPUBuffer* vbo;
        uint32_t size;
        if (type == WORLD_PASS_TYPE_OPAQUE)
        {
            vbo = chunk->opaque_vbo;
            size = chunk->opaque_size;
        }
        else
        {
            vbo = chunk->transparent_vbo;
            size = chunk->transparent_size;
        }
        if (!size)
        {
            continue;
        }
        x *= CHUNK_X;
        y *= CHUNK_Y;
        z *= CHUNK_Z;
        if (camera && !camera_test(camera, x, y, z, CHUNK_X, CHUNK_Y, CHUNK_Z))
        {
            continue;
        }
        int32_t position[3] = { x, y, z };
        SDL_PushGPUVertexUniformData(commands, 0, position, sizeof(position));
        SDL_GPUBufferBinding vbb = {0};
        vbb.buffer = vbo;
        SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);
        assert(size <= ibo_size);
        SDL_DrawGPUIndexedPrimitives(pass, size * 6, 1, 0, 0, 0);
    }
}

void world_set_block(
    const int x,
    const int y,
    const int z,
    const block_t block)
{
    const int a = floor((float) x / CHUNK_X);
    const int c = floor((float) z / CHUNK_Z);
    if (!terrain_in2(&terrain, a, c) || y < 0 || y >= GROUP_Y)
    {
        return;
    }
    const int b = y % CHUNK_Y;
    int d = x;
    int w = 0;
    int f = z;
    chunk_wrap(&d, &w, &f);
    group_t* group = terrain_get2(&terrain, a, c);
    database_set_block(a, c, d, y, f, block);
    group_set_block(group, d, y, f, block);
    const int e = y / CHUNK_Y;
    chunk_t* chunk = &group->chunks[e];
    chunk->dirty = true;
    chunk_t* neighbors[DIRECTION_3];
    get_neighbors2(a, e, c, neighbors);
    if (d == 0 && neighbors[DIRECTION_W])
    {
        neighbors[DIRECTION_W]->dirty = true;
    }
    else if (d == CHUNK_X - 1 && neighbors[DIRECTION_E])
    {
        neighbors[DIRECTION_E]->dirty = true;
    }
    if (f == 0 && neighbors[DIRECTION_S])
    {
        neighbors[DIRECTION_S]->dirty = true;
    }
    else if (f == CHUNK_Z - 1 && neighbors[DIRECTION_N])
    {
        neighbors[DIRECTION_N]->dirty = true;
    }
    if (b == 0 && neighbors[DIRECTION_D])
    {
        neighbors[DIRECTION_D]->dirty = true;
    }
    else if (b == CHUNK_Y - 1 && neighbors[DIRECTION_U])
    {
        neighbors[DIRECTION_U]->dirty = true;
    }
}

block_t world_get_block(
    const int x,
    const int y,
    const int z)
{
    const int a = floor((float) x / CHUNK_X);
    const int c = floor((float) z / CHUNK_Z);
    if (!terrain_in2(&terrain, a, c) || y < 0 || y >= GROUP_Y)
    {
        return BLOCK_EMPTY;
    }
    int d = x;
    int w = 0;
    int f = z;
    chunk_wrap(&d, &w, &f);
    const group_t* group = terrain_get2(&terrain, a, c);
    return group_get_block(group, d, y, f);
}