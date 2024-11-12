#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stb_image.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "block.h"
#include "camera.h"
#include "database.h"
#include "noise.h"
#include "pipeline.h"
#include "raycast.h"
#include "world.h"

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUCommandBuffer* commands;
static uint32_t width;
static uint32_t height;
static SDL_GPUTexture* color_texture;
static SDL_GPUTexture* depth_texture;
static SDL_GPUBuffer* quad_vbo;
static SDL_GPUBuffer* cube_vbo;
static SDL_Surface* atlas_surface;
static SDL_GPUTexture* atlas_texture;
static SDL_GPUSampler* atlas_sampler;
static void* atlas_data;
static SDL_GPUTexture* shadow_texture;
static SDL_GPUSampler* shadow_sampler;
static SDL_GPUTexture* position_texture;
static SDL_GPUTexture* uv_texture;
static SDL_GPUTexture* voxel_texture;
static SDL_GPUSampler* composite_sampler;
static SDL_GPUTexture* edge_texture;
static SDL_GPUSampler* edge_sampler;
static camera_t player_camera;
static camera_t shadow_camera;
static uint64_t time1;
static uint64_t time2;
static block_t current_block = BLOCK_GRASS;

static void load_atlas()
{
    int w;
    int h;
    int channels;
    atlas_data = stbi_load("atlas.png", &w, &h, &channels, 4);
    if (!atlas_data || channels != 4)
    {
        SDL_Log("Failed to create atlas image: %s", stbi_failure_reason());
        return;
    }
    atlas_surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, atlas_data, w * 4);
    if (!atlas_surface)
    {
        SDL_Log("Failed to create atlas surface: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureCreateInfo tci = {0};
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.layer_count_or_depth = 1;
    tci.num_levels = ATLAS_LEVELS;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tci.width = w;
    tci.height = h;
    atlas_texture = SDL_CreateGPUTexture(device, &tci);
    if (!atlas_texture)
    {
        SDL_Log("Failed to create atlas texture: %s", SDL_GetError());
        return;
    }
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.size = w * h * 4;
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!buffer)
    {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    void* data = SDL_MapGPUTransferBuffer(device, buffer, 0);
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, atlas_surface->pixels, w * h * 4);
    SDL_UnmapGPUTransferBuffer(device, buffer);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureTransferInfo tti = {0};
    tti.transfer_buffer = buffer;
    SDL_GPUTextureRegion region = {0};
    region.texture = atlas_texture;
    region.w = w;
    region.h = h;
    region.d = 1;
    SDL_UploadToGPUTexture(pass, &tti, &region, 0);
    SDL_EndGPUCopyPass(pass);
    SDL_GenerateMipmapsForGPUTexture(commands, atlas_texture);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device, buffer);
}

static void create_samplers()
{
    SDL_GPUSamplerCreateInfo sci = {0};
    sci.min_filter = SDL_GPU_FILTER_NEAREST;
    sci.mag_filter = SDL_GPU_FILTER_NEAREST;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    atlas_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!atlas_sampler)
    {
        SDL_Log("Failed to create atlas sampler: %s", SDL_GetError());
        return;
    }
    composite_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!composite_sampler)
    {
        SDL_Log("Failed to create composite sampler: %s", SDL_GetError());
        return;
    }
    edge_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!edge_sampler)
    {
        SDL_Log("Failed to create edge sampler: %s", SDL_GetError());
        return;
    }
    sci.min_filter = SDL_GPU_FILTER_LINEAR;
    sci.mag_filter = SDL_GPU_FILTER_LINEAR;
    shadow_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!shadow_sampler)
    {
        SDL_Log("Failed to create shadow sampler: %s", SDL_GetError());
        return;
    }
}

static void create_textures()
{
    SDL_GPUTextureCreateInfo tci = {0};
    tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    tci.width = SHADOW_SIZE;
    tci.height = SHADOW_SIZE;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    shadow_texture = SDL_CreateGPUTexture(device, &tci);
    if (!shadow_texture)
    {
        SDL_Log("Failed to create shadow texture: %s", SDL_GetError());
        return;
    }
}

static bool resize_textures(const uint32_t width, const uint32_t height)
{
    if (depth_texture)
    {
        SDL_ReleaseGPUTexture(device, depth_texture);
        depth_texture = NULL;
    }
    if (position_texture)
    {
        SDL_ReleaseGPUTexture(device, position_texture);
        position_texture = NULL;
    }
    if (uv_texture)
    {
        SDL_ReleaseGPUTexture(device, uv_texture);
        uv_texture = NULL;
    }
    if (voxel_texture)
    {
        SDL_ReleaseGPUTexture(device, voxel_texture);
        voxel_texture = NULL;
    }
    if (edge_texture)
    {
        SDL_ReleaseGPUTexture(device, edge_texture);
        edge_texture = NULL;
    }
    SDL_GPUTextureCreateInfo tci = {0};
    tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    tci.width = width;
    tci.height = height;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    depth_texture = SDL_CreateGPUTexture(device, &tci);
    if (!depth_texture)
    {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return false;
    }
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    position_texture = SDL_CreateGPUTexture(device, &tci);
    if (!position_texture)
    {
        SDL_Log("Failed to create position texture: %s", SDL_GetError());
        return false;
    }
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.format = SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;
    uv_texture = SDL_CreateGPUTexture(device, &tci);
    if (!uv_texture)
    {
        SDL_Log("Failed to create uv texture: %s", SDL_GetError());
        return false;
    }
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.format = SDL_GPU_TEXTUREFORMAT_R32_UINT;
    voxel_texture = SDL_CreateGPUTexture(device, &tci);
    if (!voxel_texture)
    {
        SDL_Log("Failed to create voxel texture: %s", SDL_GetError());
        return false;
    }
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    edge_texture = SDL_CreateGPUTexture(device, &tci);
    if (!edge_texture)
    {
        SDL_Log("Failed to create edge texture: %s", SDL_GetError());
        return false;
    }
    return true;
}

SDL_Surface* create_icon(const block_t block)
{
    assert(block < BLOCK_COUNT);
    if (!atlas_surface)
    {
        return NULL;
    }
    SDL_Surface* icon = SDL_CreateSurface(ATLAS_FACE_WIDTH,
        ATLAS_FACE_HEIGHT, SDL_PIXELFORMAT_RGBA32);
    if (!icon)
    {
        SDL_Log("Failed to create icon surface: %s", SDL_GetError());
        return NULL;
    }
    SDL_Rect src;
    src.x = blocks[block][0][0] * ATLAS_FACE_WIDTH;
    src.y = blocks[block][0][1] * ATLAS_FACE_HEIGHT;
    src.w = ATLAS_FACE_WIDTH;
    src.h = ATLAS_FACE_HEIGHT;
    SDL_Rect dst;
    dst.x = 0;
    dst.y = 0;
    dst.w = ATLAS_FACE_WIDTH;
    dst.h = ATLAS_FACE_HEIGHT;
    if (!SDL_BlitSurface(atlas_surface, &src, icon, &dst))
    {
        SDL_Log("Failed to blit icon surface: %s", SDL_GetError());
        SDL_DestroySurface(icon);
        return NULL;
    }
    return icon;
}

static void create_vbos()
{
    const float quad[][2] =
    {
        {-1,-1},
        { 1,-1},
        {-1, 1},
        { 1, 1},
        { 1,-1},
        {-1, 1},
    };
    const float cube[][3] =
    {
        {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1},
        {-1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
        { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1},
        { 1,-1, 1}, {-1, 1, 1}, {-1,-1, 1},
        {-1,-1,-1}, {-1, 1,-1}, {-1, 1, 1},
        {-1,-1,-1}, {-1, 1, 1}, {-1,-1, 1},
        { 1,-1,-1}, { 1,-1, 1}, { 1, 1, 1},
        { 1,-1,-1}, { 1, 1, 1}, { 1, 1,-1},
        {-1, 1,-1}, { 1, 1,-1}, { 1, 1, 1},
        {-1, 1,-1}, { 1, 1, 1}, {-1, 1, 1},
        {-1,-1,-1}, {-1,-1, 1}, { 1,-1, 1},
        {-1,-1,-1}, { 1,-1, 1}, { 1,-1,-1},
    };
    SDL_GPUBufferCreateInfo bci = {0};
    bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bci.size = sizeof(quad);
    quad_vbo = SDL_CreateGPUBuffer(device, &bci);
    if (!quad_vbo)
    {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return;
    }
    bci.size = sizeof(cube);
    cube_vbo = SDL_CreateGPUBuffer(device, &bci);
    if (!cube_vbo)
    {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return;
    }
    void* data;
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = sizeof(quad);
    SDL_GPUTransferBuffer* qtbo = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!qtbo)
    {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    data = SDL_MapGPUTransferBuffer(device, qtbo, false);
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, quad, sizeof(quad));
    tbci.size = sizeof(cube);
    SDL_GPUTransferBuffer* ctbo = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!ctbo)
    {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    data = SDL_MapGPUTransferBuffer(device, ctbo, false);
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, cube, sizeof(cube));
    SDL_UnmapGPUTransferBuffer(device, ctbo);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return;
    } 
    SDL_GPUTransferBufferLocation location = {0};
    SDL_GPUBufferRegion region = {0};
    location.transfer_buffer = qtbo;
    region.size = sizeof(quad);
    region.buffer = quad_vbo;
    SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    location.transfer_buffer = ctbo;
    region.size = sizeof(cube);
    region.buffer = cube_vbo;
    SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands); 
    SDL_ReleaseGPUTransferBuffer(device, qtbo);
    SDL_ReleaseGPUTransferBuffer(device, ctbo);
}

static void draw_raycast()
{
    float x;
    float y;
    float z;
    float a;
    float b;
    float c;
    camera_get_position(&player_camera, &x, &y, &z);
    camera_vector(&player_camera, &a, &b, &c);
    if (!raycast(&x, &y, &z, a, b, c, RAYCAST_LENGTH, false))
    {
        return;
    }
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.load_op = SDL_GPU_LOADOP_LOAD;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.texture = depth_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, &dsti);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    int32_t position[3] = { x, y, z };
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = cube_vbo;
    pipeline_bind(pass, PIPELINE_RAYCAST);
    SDL_PushGPUVertexUniformData(commands, 0, player_camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 1, position, 12);
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_sky()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_DONT_CARE;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = cube_vbo;
    SDL_PushGPUVertexUniformData(commands, 0, player_camera.view, 64);
    SDL_PushGPUVertexUniformData(commands, 1, player_camera.proj, 64);
    pipeline_bind(pass, PIPELINE_SKY);
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_ui()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    int32_t viewport[2] = { width, height };
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = quad_vbo;
    pipeline_bind(pass, PIPELINE_UI);
    if (atlas_sampler && atlas_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = atlas_sampler;
        tsb.texture = atlas_texture;
        SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    }
    SDL_PushGPUFragmentUniformData(commands, 0, &viewport, 8);
    SDL_PushGPUFragmentUniformData(commands, 1, blocks[current_block][0], 8);
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_edge()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.clear_color = (SDL_FColor) { 0.0f, 0.0f, 0.0f, 0.0f };
    cti.load_op = SDL_GPU_LOADOP_CLEAR;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = edge_texture;
    cti.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = quad_vbo;
    pipeline_bind(pass, PIPELINE_EDGE);
    if (edge_sampler && position_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = edge_sampler;
        tsb.texture = position_texture;
        SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    }
    if (edge_sampler && uv_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = edge_sampler;
        tsb.texture = uv_texture;
        SDL_BindGPUFragmentSamplers(pass, 1, &tsb, 1);
    }
    if (edge_sampler && voxel_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = edge_sampler;
        tsb.texture = voxel_texture;
        SDL_BindGPUFragmentSamplers(pass, 2, &tsb, 1);
    }
    if (atlas_sampler && voxel_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = edge_sampler;
        tsb.texture = atlas_texture;
        SDL_BindGPUFragmentSamplers(pass, 3, &tsb, 1);
    }
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_opaque()
{
    SDL_GPUColorTargetInfo cti[3] = {0};
    cti[0].load_op = SDL_GPU_LOADOP_DONT_CARE;
    cti[0].store_op = SDL_GPU_STOREOP_STORE;
    cti[0].texture = position_texture;
    cti[0].cycle = true;
    cti[1].clear_color = (SDL_FColor) { 0.0f, 0.0f, 0.0f, 0.0f };
    cti[1].load_op = SDL_GPU_LOADOP_CLEAR;
    cti[1].store_op = SDL_GPU_STOREOP_STORE;
    cti[1].texture = uv_texture;
    cti[2].cycle = true;
    cti[2].load_op = SDL_GPU_LOADOP_DONT_CARE;
    cti[2].store_op = SDL_GPU_STOREOP_STORE;
    cti[2].texture = voxel_texture;
    cti[2].cycle = true;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.clear_depth = 1.0f;
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    dsti.texture = depth_texture;
    dsti.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, cti, 3, &dsti);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    float position[3];
    float vector[3];
    float planes[2] = { player_camera.near, player_camera.far };
    camera_get_position(&player_camera, &position[0], &position[1], &position[2]);
    camera_vector(&shadow_camera, &vector[0], &vector[1], &vector[2]);
    pipeline_bind(pass, PIPELINE_OPAQUE);
    if (atlas_sampler && atlas_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = atlas_sampler;
        tsb.texture = atlas_texture;
        SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    }
    SDL_PushGPUVertexUniformData(commands, 1, player_camera.view, 64);
    SDL_PushGPUVertexUniformData(commands, 2, player_camera.proj, 64);
    world_render(&player_camera, commands, pass, true);
    SDL_EndGPURenderPass(pass);
}

static void draw_transparent()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.load_op = SDL_GPU_LOADOP_LOAD;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.texture = depth_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, &dsti);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    float position[3];
    float vector[3];
    camera_get_position(&player_camera, &position[0], &position[1], &position[2]);
    camera_vector(&shadow_camera, &vector[0], &vector[1], &vector[2]);
    pipeline_bind(pass, PIPELINE_TRANSPARENT);
    SDL_PushGPUVertexUniformData(commands, 1, player_camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 2, position, 12);
    SDL_PushGPUVertexUniformData(commands, 3, shadow_camera.matrix, 64);
    SDL_PushGPUFragmentUniformData(commands, 0, vector, 12);
    if (atlas_sampler && atlas_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = atlas_sampler;
        tsb.texture = atlas_texture;
        SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    }
    if (shadow_sampler && shadow_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = shadow_sampler;
        tsb.texture = shadow_texture;
        SDL_BindGPUFragmentSamplers(pass, 1, &tsb, 1);
    }
    world_render(&player_camera, commands, pass, false);
    SDL_EndGPURenderPass(pass);
}

static void draw_shadow()
{
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.clear_depth = 1.0f;
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    dsti.texture = shadow_texture;
    dsti.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, NULL, 0, &dsti);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    pipeline_bind(pass, PIPELINE_SHADOW);
    SDL_PushGPUVertexUniformData(commands, 1, shadow_camera.matrix, 64);
    world_render(NULL, commands, pass, true);
    SDL_EndGPURenderPass(pass);
}

static void draw_composite()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    int32_t viewport[2] = { width, height };
    float player_position[3];
    float shadow_vector[3];
    camera_get_position(&player_camera, &player_position[0], &player_position[1], &player_position[2]);
    camera_vector(&shadow_camera, &shadow_vector[0], &shadow_vector[1], &shadow_vector[2]);
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = quad_vbo;
    pipeline_bind(pass, PIPELINE_COMPOSITE);
    if (atlas_sampler && atlas_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = atlas_sampler;
        tsb.texture = atlas_texture;
        SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    }
    if (composite_sampler && position_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = composite_sampler;
        tsb.texture = position_texture;
        SDL_BindGPUFragmentSamplers(pass, 1, &tsb, 1);
    }
    if (composite_sampler && uv_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = composite_sampler;
        tsb.texture = uv_texture;
        SDL_BindGPUFragmentSamplers(pass, 2, &tsb, 1);
    }
    if (composite_sampler && voxel_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = composite_sampler;
        tsb.texture = voxel_texture;
        SDL_BindGPUFragmentSamplers(pass, 3, &tsb, 1);
    }
    if (shadow_sampler && shadow_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = shadow_sampler;
        tsb.texture = shadow_texture;
        SDL_BindGPUFragmentSamplers(pass, 4, &tsb, 1);
    }
    if (edge_sampler && edge_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = edge_sampler;
        tsb.texture = edge_texture;
        SDL_BindGPUFragmentSamplers(pass, 5, &tsb, 1);
    }
    SDL_PushGPUFragmentUniformData(commands, 0, &viewport, 8);
    SDL_PushGPUFragmentUniformData(commands, 1, player_position, 12);
    SDL_PushGPUFragmentUniformData(commands, 2, shadow_vector, 12);
    SDL_PushGPUFragmentUniformData(commands, 3, shadow_camera.matrix, 64);
    SDL_PushGPUFragmentUniformData(commands, 4, player_camera.view, 64);
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw()
{
    commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    uint32_t w;
    uint32_t h;
    if (!SDL_AcquireGPUSwapchainTexture(commands, window, &color_texture, &w, &h))
    {
        SDL_Log("Failed to aqcuire swapchain image: %s", SDL_GetError());
        return;
    }
    if (w == 0 || h == 0)
    {
        SDL_SubmitGPUCommandBuffer(commands);
        return;
    }
    if (width != w || height != h)
    {
        if (resize_textures(w, h))
        {
            width = w;
            height = h;
        }
        else
        {
            SDL_Log("Failed to resize textures");
            return;
        }
    }
    camera_update(&player_camera);
    camera_update(&shadow_camera);
    draw_sky();
    draw_shadow();
    draw_opaque();
    draw_edge();
    draw_composite();
    draw_transparent();
    draw_raycast();
    draw_ui();
    SDL_SubmitGPUCommandBuffer(commands);
}

static bool poll()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_WINDOW_RESIZED:
            camera_viewport(&player_camera, event.window.data1, event.window.data2);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (SDL_GetWindowRelativeMouseMode(window))
            {
                const float yaw = event.motion.xrel * PLAYER_SENSITIVITY;
                const float pitch = -event.motion.yrel * PLAYER_SENSITIVITY;
                camera_rotate(&player_camera, pitch, yaw);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (!SDL_GetWindowRelativeMouseMode(window))
            {
                SDL_SetWindowRelativeMouseMode(window, true);
                break;
            }
            if (event.button.button == BUTTON_PLACE ||
                event.button.button == BUTTON_BREAK)
            {
                bool previous = true;
                block_t block = current_block;
                if (event.button.button == BUTTON_BREAK)
                {
                    previous = false;
                    block = BLOCK_EMPTY;
                }
                float x, y, z;
                float a, b, c;
                camera_get_position(&player_camera, &x, &y, &z);
                camera_vector(&player_camera, &a, &b, &c);
                if (raycast(&x, &y, &z, a, b, c, RAYCAST_LENGTH, previous) && y >= 1.0f)
                {
                    world_set_block(x, y, z, block);
                }
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == BUTTON_PAUSE)
            {
                SDL_SetWindowRelativeMouseMode(window, false);
                SDL_SetWindowFullscreen(window, false);
            }
            else if (event.key.scancode == BUTTON_BLOCK)
            {
                current_block = (current_block + 1) % BLOCK_COUNT;
                current_block = clamp(current_block, BLOCK_EMPTY + 1, BLOCK_COUNT - 1);
            }
            else if (event.key.scancode == BUTTON_FULLSCREEN)
            {
                if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)
                {
                    SDL_SetWindowFullscreen(window, false);
                    SDL_SetWindowRelativeMouseMode(window, false);
                }
                else
                {
                    SDL_SetWindowFullscreen(window, true);
                    SDL_SetWindowRelativeMouseMode(window, true);
                }
            }
            break;
        case SDL_EVENT_QUIT:
            return false;
        }
    }
    return true;
}

static void move(const float dt)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float speed = PLAYER_SPEED;
    const bool* state = SDL_GetKeyboardState(NULL);
    x += state[BUTTON_RIGHT];
    x -= state[BUTTON_LEFT];
    y += state[BUTTON_UP];
    y -= state[BUTTON_DOWN];
    z += state[BUTTON_FORWARD];
    z -= state[BUTTON_BACKWARD];
    if (state[BUTTON_SPRINT])
    {
        speed = PLAYER_SUPER_SPEED;
    }
    x *= speed * dt;
    y *= speed * dt;
    z *= speed * dt;
    camera_move(&player_camera, x, y, z);
    camera_get_position(&player_camera, &x, &y, &z);
    int a = x;
    int c = z;
    a /= CHUNK_X;
    c /= CHUNK_Z;
    a *= CHUNK_X;
    c *= CHUNK_Z;
    camera_set_position(&shadow_camera, a, SHADOW_Y, c);
}

static void commit()
{
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    camera_get_position(&player_camera, &x, &y, &z);
    camera_get_rotation(&player_camera, &pitch, &yaw);
    database_set_player(DATABASE_PLAYER, x, y, z, pitch, yaw);
    database_commit();
}

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    SDL_SetAppMetadata(APP_NAME, APP_VERSION, NULL);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window)
    {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, APP_VALIDATION, NULL);
    if (!device)
    {
        SDL_Log("Failed to create device: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_Log("Failed to create swapchain: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    if (!pipeline_init(device, window))
    {
        SDL_Log("Failed to create pipelines");
        return EXIT_FAILURE;
    }
    load_atlas();
    create_samplers();
    create_textures();
    create_vbos();
    SDL_SetWindowResizable(window, true);
    SDL_Surface* icon = create_icon(APP_ICON);
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
    if (!database_init(DATABASE_PATH))
    {
        SDL_Log("Failed to create database");
        return false;
    }
    if (!world_init(device))
    {
        SDL_Log("Failed to create world");
        return EXIT_FAILURE;
    }
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    camera_init(&player_camera, false);
    camera_viewport(&player_camera, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (database_get_player(DATABASE_PLAYER, &x, &y, &z, &pitch, &yaw))
    {
        camera_set_position(&player_camera, x, y, z);
        camera_set_rotation(&player_camera, pitch, yaw);
    }
    else
    {
        srand(time(NULL));
        camera_set_position(&player_camera, rand(), PLAYER_Y, rand());
    }
    camera_init(&shadow_camera, true);
    camera_set_rotation(&shadow_camera, SHADOW_PITCH, SHADOW_YAW);
    move(0.0f);
    int cooldown = 0;
    time1 = SDL_GetPerformanceCounter();
    time2 = 0;
    while (1)
    {
        time2 = time1;
        time1 = SDL_GetPerformanceCounter();
        const float frequency = SDL_GetPerformanceFrequency();
        const float dt = (time1 - time2) * 1000 / frequency;
        if (!poll())
        {
            break;
        }
        move(dt);
        camera_get_position(&player_camera, &x, &y, &z);
        world_update(x, y, z);
        draw();
        if (cooldown++ > DATABASE_TIME)
        {
            commit();
            cooldown = 0;
        }
    }
    world_free();
    commit();
    database_free();
    if (cube_vbo)
    {
        SDL_ReleaseGPUBuffer(device, cube_vbo);
    }
    if (quad_vbo)
    {
        SDL_ReleaseGPUBuffer(device, quad_vbo);
    }
    if (depth_texture)
    {
        SDL_ReleaseGPUTexture(device, depth_texture);
    }
    if (atlas_surface)
    {
        SDL_DestroySurface(atlas_surface);
    }
    if (atlas_texture)
    {
        SDL_ReleaseGPUTexture(device, atlas_texture);
    }
    if (atlas_sampler)
    {
        SDL_ReleaseGPUSampler(device, atlas_sampler);
    }
    if (shadow_texture)
    {
        SDL_ReleaseGPUTexture(device, shadow_texture);
    }
    if (shadow_sampler)
    {
        SDL_ReleaseGPUSampler(device, shadow_sampler);
    }
    if (position_texture)
    {
        SDL_ReleaseGPUTexture(device, position_texture);
    }
    if (uv_texture)
    {
        SDL_ReleaseGPUTexture(device, uv_texture);
    }
    if (voxel_texture)
    {
        SDL_ReleaseGPUTexture(device, voxel_texture);
    }
    if (composite_sampler)
    {
        SDL_ReleaseGPUSampler(device, composite_sampler);
    }
    if (edge_sampler)
    {
        SDL_ReleaseGPUSampler(device, edge_sampler);
    }
    if (edge_texture)
    {
        SDL_ReleaseGPUTexture(device, edge_texture);
    }
    pipeline_free();
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    stbi_image_free(atlas_data);
    return EXIT_SUCCESS;
}