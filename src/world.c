#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// #include <threads.h>
#include "tinycthread.h"
#include "block.h"
#include "camera.h"
#include "chunk.h"
#include "database.h"
#include "helpers.h"
#include "noise.h"
#include "voxel.h"
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
    int z;
}
job_t;

typedef struct
{
    thrd_t thrd;
    mtx_t mtx;
    cnd_t cnd;
    const job_t* job;
    SDL_GPUTransferBuffer* tbos[CHUNK_MESH_COUNT];
    uint32_t sizes[CHUNK_MESH_COUNT];
}
worker_t;

static terrain_t terrain;
static SDL_GPUDevice* device;
static SDL_GPUBuffer* ibo;
static uint32_t ibo_size;
static worker_t workers[WORLD_WORKERS];
static int sorted[WORLD_CHUNKS][2];

static int loop(
    void* args)
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
        const int z = terrain.z + worker->job->z;
        chunk_t* chunk = terrain_get2(&terrain, x, z);
        switch (worker->job->type)
        {
        case JOB_TYPE_LOAD:
            assert(chunk->load);
            noise_generate(chunk, x, z);
            database_get_blocks(chunk, x, z);
            chunk->load = false;
            break;
        case JOB_TYPE_MESH:
            assert(!chunk->skip);
            assert(!chunk->load);
            assert(chunk->mesh);
            chunk_t* neighbors[DIRECTION_2];
            terrain_neighbors2(&terrain, x, z, neighbors);
            chunk->mesh = !voxel_vbo(
                chunk,
                neighbors,
                device,
                worker->tbos,
                worker->sizes);
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

static void dispatch(
    worker_t* worker,
    const job_t* job)
{
    assert(worker);
    assert(job);
    mtx_lock(&worker->mtx);
    assert(!worker->job);
    worker->job = job;
    cnd_signal(&worker->cnd);
    mtx_unlock(&worker->mtx);
}

static void wait_for_worker(
    worker_t* worker)
{
    assert(worker);
    mtx_lock(&worker->mtx);
    while (worker->job)
    {
        cnd_wait(&worker->cnd, &worker->mtx);
    }
    mtx_unlock(&worker->mtx);
}

bool world_init(
    SDL_GPUDevice* handle)
{
    assert(handle);
    device = handle;
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
    int i = 0;
    for (int x = 0; x < WORLD_X; x++)
    for (int z = 0; z < WORLD_Z; z++)
    {
        sorted[i][0] = x;
        sorted[i][1] = z;
        i++;
    }
    const int w = WORLD_X / 2;
    const int h = WORLD_Z / 2;
    sort_2d(w, h, sorted, WORLD_CHUNKS);
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
    for (int x = 0; x < WORLD_X; x++)
    for (int z = 0; z < WORLD_Z; z++)
    {
        chunk_t* chunk = terrain_get(&terrain, x, z);
        for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
        {
            if (chunk->vbos[mesh])
            {
                SDL_ReleaseGPUBuffer(device, chunk->vbos[mesh]);
                chunk->vbos[mesh] = NULL;
            }
        }
    }
    terrain_free(&terrain);
    for (int i = 0; i < WORLD_WORKERS; i++)
    {
        worker_t* worker = &workers[i];
        thrd_join(worker->thrd, NULL);
        mtx_destroy(&worker->mtx);
        cnd_destroy(&worker->cnd);
        for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
        {
            if (worker->tbos[mesh])
            {
                SDL_ReleaseGPUTransferBuffer(device, worker->tbos[mesh]);
                worker->tbos[mesh] = NULL;
            }
        }
    }
    if (ibo)
    {
        SDL_ReleaseGPUBuffer(device, ibo);
        ibo = NULL;
    }
    device = NULL;
}

static void move(
    const int x,
    const int y,
    const int z)
{
    const int a = x / CHUNK_X - WORLD_X / 2;
    const int c = z / CHUNK_Z - WORLD_Z / 2;
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
        chunk_t* chunk = terrain_get(&terrain, j, k);
        memset(chunk->blocks, 0, sizeof(chunk->blocks));
        chunk->skip = true;
        chunk->load = true;
        chunk->mesh = true;
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
    for (int i = 0; i < WORLD_CHUNKS && n < WORLD_WORKERS; i++)
    {
        const int j = sorted[i][0];
        const int k = sorted[i][1];
        chunk_t* chunk = terrain_get(&terrain, j, k);
        if (chunk->load)
        {
            job_t* job = &jobs[n++];
            job->type = JOB_TYPE_LOAD;
            job->x = j;
            job->z = k;
            continue;
        }
        if (chunk->skip || !chunk->mesh || terrain_border(&terrain, j, k))
        {
            continue;
        }
        bool status = true;
        chunk_t* neighbors[DIRECTION_2];
        terrain_neighbors(&terrain, j, k, neighbors);
        for (direction_t direction = 0; direction < DIRECTION_2; direction++)
        {
            const chunk_t* neighbor = neighbors[direction];
            if (!neighbor || neighbor->load)
            {
                status = false;
                break;
            }
        }
        if (status)
        {
            job_t* job = &jobs[n++];
            job->type = JOB_TYPE_MESH;
            job->x = j;
            job->z = k;
            continue;
        }
    }
    uint32_t size = 0;
    for (int i = 0; i < n; i++)
    {
        dispatch(&workers[i], &jobs[i]);
    }
    for (int i = 0; i < n; i++)
    {
        wait_for_worker(&workers[i]);
    }
    for (int i = 0; i < n; i++)
    {
        const job_t* job = &jobs[i];
        if (job->type != JOB_TYPE_MESH)
        {
            continue;
        }
        chunk_t* chunk = terrain_get(&terrain, job->x, job->z);
        for (chunk_mesh_t mesh = 0; mesh < CHUNK_MESH_COUNT; mesh++)
        {
            size = max(size, chunk->sizes[mesh]);
        }
    }
    if (size > ibo_size)
    {
        if (ibo)
        {
            SDL_ReleaseGPUBuffer(device, ibo);
            ibo = NULL;
            ibo_size = 0;
        }
        if (voxel_ibo(device, &ibo, size))
        {
            ibo_size = size;
        }
    }
}

void world_render(
    const camera_t* camera,
    SDL_GPUCommandBuffer* commands,
    SDL_GPURenderPass* pass,
    const chunk_mesh_t mesh)
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
        int x;
        int z;
        if (mesh == CHUNK_MESH_OPAQUE)
        {
            x = sorted[i][0] + terrain.x;
            z = sorted[i][1] + terrain.z;
        }
        else
        {
            x = sorted[WORLD_CHUNKS - i - 1][0] + terrain.x;
            z = sorted[WORLD_CHUNKS - i - 1][1] + terrain.z;
        }
        const chunk_t* chunk = terrain_get2(&terrain, x, z);
        if (terrain_border2(&terrain, x, z)) 
        {
            continue;
        }
        if (chunk->skip || chunk->mesh || !chunk->sizes[mesh])
        {
            continue;
        }
        assert(chunk->sizes[mesh] <= ibo_size);
        x *= CHUNK_X;
        z *= CHUNK_Z;
        if (camera && !camera_test(camera, x, 0, z, CHUNK_X, CHUNK_Y, CHUNK_Z))
        {
            continue;
        }
        int32_t position[3] = { x, 0, z };
        SDL_GPUBufferBinding vbb = {0};
        vbb.buffer = chunk->vbos[mesh];
        SDL_PushGPUVertexUniformData(commands, 0, position, sizeof(position));
        SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);
        SDL_DrawGPUIndexedPrimitives(pass, chunk->sizes[mesh] * 6, 1, 0, 0, 0);

    }
}

void world_set_block(
    int x,
    int y,
    int z,
    const block_t block)
{
    const int a = floor((float) x / CHUNK_X);
    const int c = floor((float) z / CHUNK_Z);
    if (!terrain_in2(&terrain, a, c) || y < 0 || y >= CHUNK_Y)
    {
        return;
    }
    chunk_wrap(&x, &y, &z);
    chunk_t* chunk = terrain_get2(&terrain, a, c);
    database_set_block(a, c, x, y, z, block);
    chunk_set_block(chunk, x, y, z, block);
    chunk->mesh = true;
    chunk_t* neighbors[DIRECTION_2];
    terrain_neighbors2(&terrain, a, c, neighbors);
    if (x == 0 && neighbors[DIRECTION_W])
    {
        neighbors[DIRECTION_W]->mesh = true;
    }
    else if (x == CHUNK_X - 1 && neighbors[DIRECTION_E])
    {
        neighbors[DIRECTION_E]->mesh = true;
    }
    if (z == 0 && neighbors[DIRECTION_S])
    {
        neighbors[DIRECTION_S]->mesh = true;
    }
    else if (z == CHUNK_Z - 1 && neighbors[DIRECTION_N])
    {
        neighbors[DIRECTION_N]->mesh = true;
    }
}

block_t world_get_block(
    int x,
    int y,
    int z)
{
    const int a = floor((float) x / CHUNK_X);
    const int c = floor((float) z / CHUNK_Z);
    if (!terrain_in2(&terrain, a, c) || y < 0 || y >= CHUNK_Y)
    {
        return BLOCK_EMPTY;
    }
    chunk_wrap(&x, &y, &z);
    const chunk_t* chunk = terrain_get2(&terrain, a, c);
    if (!chunk->load)
    {
        return chunk_get_block(chunk, x, y, z);
    }
    else
    {
        return BLOCK_EMPTY;
    }
}