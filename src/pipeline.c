#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "helpers.h"
#include "pipeline.h"

static SDL_GPUDevice* device;
static SDL_GPUGraphicsPipeline* pipelines[PIPELINE_COUNT];

static SDL_GPUShader* load(
    SDL_GPUDevice* device,
    const char* file,
    const int uniforms,
    const int samplers)
{
    assert(device);
    assert(file);
    SDL_GPUShaderCreateInfo info = {0};
    void* code = SDL_LoadFile(file, &info.code_size);
    if (!code)
    {
        SDL_Log("Failed to load %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    info.code = code;
    if (strstr(file, ".vert"))
    {
        info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    }
    else
    {
        info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.entrypoint = "main";
    info.num_uniform_buffers = uniforms;
    info.num_samplers = samplers;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    SDL_free(code);
    if (!shader)
    {
        SDL_Log("Failed to create %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    return shader;
}

static SDL_GPUGraphicsPipeline* sky(SDL_Window* window)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load(device, "sky.vert", 2, 0),
        .fragment_shader = load(device, "sky.frag", 0, 0),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window)
            }},
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 12,
            }},
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create sky pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* shadow(SDL_Window* window)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load(device, "shadow.vert", 2, 0),
        .fragment_shader = load(device, "shadow.frag", 0, 0),
        .target_info =
        {
            .has_depth_stencil_target = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 4,
            }},
        },
        .depth_stencil_state =
        {
            .enable_depth_test = 1,
            .enable_depth_write = 1,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state =
        {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
            .fill_mode = SDL_GPU_FILLMODE_FILL,
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* opaque(SDL_Window* window)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load(device, "opaque.vert", 3, 0),
        .fragment_shader = load(device, "opaque.frag", 0, 1),
        .target_info =
        {
            .num_color_targets = 3,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
            },
            {
                .format = SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT,
            },
            {
                .format = SDL_GPU_TEXTUREFORMAT_R32_UINT,
            }},
            .has_depth_stencil_target = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 4,
            }},
        },
        .depth_stencil_state =
        {
            .enable_depth_test = 1,
            .enable_depth_write = 1,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state =
        {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
            .fill_mode = SDL_GPU_FILLMODE_FILL,
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create opaque pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* edge(SDL_Window* window)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load(device, "edge.vert", 0, 0),
        .fragment_shader = load(device, "edge.frag", 0, 4),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT
            }}
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 8,
            }},
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create edge pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* composite(SDL_Window* window)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load(device, "composite.vert", 0, 0),
        .fragment_shader = load(device, "composite.frag", 3, 6),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window) 
            }},
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 8,
            }},
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create composite pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* transparent(SDL_Window* window)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load(device, "transparent.vert", 4, 0),
        .fragment_shader = load(device, "transparent.frag", 1, 2),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
                .blend_state =
                {
                    .enable_blend = 1,
                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                },
            }},
            .has_depth_stencil_target = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 4,
            }},
        },
        .depth_stencil_state =
        {
            .enable_depth_test = 1,
            .enable_depth_write = 0,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state =
        {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create transparent pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* raycast(SDL_Window* window)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load(device, "raycast.vert", 2, 0),
        .fragment_shader = load(device, "raycast.frag", 0, 0),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
                .blend_state =
                {
                    .enable_blend = 1,
                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                },
            }},
            .has_depth_stencil_target = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 12,
            }},
        },
        .depth_stencil_state =
        {
            .enable_depth_test = 1,
            .enable_depth_write = 1,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create raycast pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

static SDL_GPUGraphicsPipeline* ui(SDL_Window* window)
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load(device, "ui.vert", 0, 0),
        .fragment_shader = load(device, "ui.frag", 2, 1),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
                .blend_state =
                {
                    .enable_blend = 1,
                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                },
            }},
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 8,
            }},
        },
    };
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    if (info.vertex_shader && info.fragment_shader)
    {
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!pipeline)
    {
        SDL_Log("Failed to create ui pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return pipeline;
}

bool pipeline_init(
    SDL_GPUDevice* handle,
    SDL_Window* window)
{
    assert(handle);
    assert(window);
    device = handle;
    pipelines[PIPELINE_SKY] = sky(window);
    pipelines[PIPELINE_SHADOW] = shadow(window);
    pipelines[PIPELINE_OPAQUE] = opaque(window);
    pipelines[PIPELINE_EDGE] = edge(window);
    pipelines[PIPELINE_COMPOSITE] = composite(window);
    pipelines[PIPELINE_TRANSPARENT] = transparent(window);
    pipelines[PIPELINE_RAYCAST] = raycast(window);
    pipelines[PIPELINE_UI] = ui(window);
    for (pipeline_t pipeline = 0; pipeline < PIPELINE_COUNT; pipeline++)
    {
        if (!pipelines[pipeline])
        {
            return false;
        }
    }
    return true;
}

void pipeline_free()
{
    for (pipeline_t pipeline = 0; pipeline < PIPELINE_COUNT; pipeline++)
    {
        if (pipelines[pipeline])
        {
            SDL_ReleaseGPUGraphicsPipeline(device, pipelines[pipeline]);
            pipelines[pipeline] = NULL;
        }
    }
    device = NULL;
}

void pipeline_bind(
    SDL_GPURenderPass* pass,
    const pipeline_t pipeline)
{
    assert(pass);
    assert(pipeline < PIPELINE_COUNT);
    SDL_BindGPUGraphicsPipeline(pass, pipelines[pipeline]);
}