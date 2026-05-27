/*
 * resource_tracker.h — Handle → Vulkan Object Mapping
 *
 * Maintains a hash map from guest-assigned 32-bit handles to
 * host-side Vulkan objects (buffers, textures, pipelines, etc.)
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#ifndef GRAVITY_RESOURCE_TRACKER_H
#define GRAVITY_RESOURCE_TRACKER_H

#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../protocol/include/gravity_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════
 * Resource Types
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum gravity_resource_type {
    GRAVITY_RES_BUFFER,
    GRAVITY_RES_TEXTURE,
    GRAVITY_RES_SAMPLER,
    GRAVITY_RES_SHADER,
    GRAVITY_RES_RENDER_PIPELINE,
    GRAVITY_RES_COMPUTE_PIPELINE,
    GRAVITY_RES_DEPTH_STENCIL,
    GRAVITY_RES_FENCE,
    GRAVITY_RES_EVENT,
    GRAVITY_RES_CMDBUF,
} gravity_resource_type_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Resource Entries (type-specific Vulkan object storage)
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_buffer_resource {
    VkBuffer        buffer;
    VkDeviceMemory  memory;
    VkDeviceSize    size;
    void*           mapped;     /* Persistently mapped pointer (for shared/managed) */
} gravity_buffer_resource_t;

typedef struct gravity_texture_resource {
    VkImage         image;
    VkImageView     image_view;
    VkDeviceMemory  memory;
    uint32_t        width, height, depth;
    uint32_t        mip_levels;
    uint32_t        array_layers;
    VkFormat        format;
    VkImageLayout   current_layout;
} gravity_texture_resource_t;

typedef struct gravity_sampler_resource {
    VkSampler       sampler;
} gravity_sampler_resource_t;

typedef struct gravity_shader_resource {
    VkShaderModule  module;
    uint32_t        shader_type;    /* gravity_shader_type_t */
} gravity_shader_resource_t;

typedef struct gravity_render_pipeline_resource {
    VkPipeline          pipeline;
    VkPipelineLayout    layout;
    VkDescriptorSetLayout desc_set_layout;
} gravity_render_pipeline_resource_t;

typedef struct gravity_compute_pipeline_resource {
    VkPipeline          pipeline;
    VkPipelineLayout    layout;
    VkDescriptorSetLayout desc_set_layout;
} gravity_compute_pipeline_resource_t;

typedef struct gravity_depth_stencil_resource {
    /* Depth/stencil state is baked into Vulkan pipelines via dynamic state.
     * We store the parameters here for use when creating pipelines. */
    gravity_compare_function_t  depth_compare;
    bool                        depth_write_enabled;
    gravity_stencil_desc_t      front_face;
    gravity_stencil_desc_t      back_face;
} gravity_depth_stencil_resource_t;

typedef struct gravity_cmdbuf_resource {
    VkCommandBuffer     cmd_buf;
    VkFence             fence;          /* Signaled when execution completes */
    bool                recording;
    bool                in_render_pass;
} gravity_cmdbuf_resource_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Unified Resource Entry
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_resource_entry {
    gravity_handle_t        handle;
    gravity_resource_type_t type;
    uint32_t                ref_count;
    bool                    in_use;

    union {
        gravity_buffer_resource_t           buffer;
        gravity_texture_resource_t          texture;
        gravity_sampler_resource_t          sampler;
        gravity_shader_resource_t           shader;
        gravity_render_pipeline_resource_t  render_pipeline;
        gravity_compute_pipeline_resource_t compute_pipeline;
        gravity_depth_stencil_resource_t    depth_stencil;
        gravity_cmdbuf_resource_t           cmdbuf;
    };
} gravity_resource_entry_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Resource Tracker
 * ═══════════════════════════════════════════════════════════════════════ */

#define GRAVITY_TRACKER_HASH_SIZE   4096
#define GRAVITY_TRACKER_MAX_ENTRIES 65536

typedef struct gravity_resource_tracker {
    gravity_resource_entry_t    entries[GRAVITY_TRACKER_MAX_ENTRIES];
    uint32_t                    hash_table[GRAVITY_TRACKER_HASH_SIZE];
    uint32_t                    entry_count;
    uint32_t                    free_head;  /* Free list head index */
} gravity_resource_tracker_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Initialize the resource tracker.
 */
void gravity_resource_tracker_init(gravity_resource_tracker_t* tracker);

/**
 * Destroy the tracker and all tracked resources.
 * NOTE: Vulkan device must still be valid when calling this.
 */
void gravity_resource_tracker_destroy(gravity_resource_tracker_t* tracker);

/**
 * Add a resource entry for the given handle.
 * Returns pointer to the entry for the caller to fill in, or NULL on error.
 */
gravity_resource_entry_t* gravity_resource_tracker_add(
    gravity_resource_tracker_t* tracker,
    gravity_handle_t handle,
    gravity_resource_type_t type);

/**
 * Look up a resource by handle.
 * Returns NULL if not found.
 */
gravity_resource_entry_t* gravity_resource_tracker_get(
    const gravity_resource_tracker_t* tracker,
    gravity_handle_t handle);

/**
 * Remove and destroy a resource.
 * The Vulkan objects are NOT destroyed here — caller must handle that.
 */
void gravity_resource_tracker_remove(
    gravity_resource_tracker_t* tracker,
    gravity_handle_t handle);

/**
 * Get resource count.
 */
uint32_t gravity_resource_tracker_count(const gravity_resource_tracker_t* tracker);

#ifdef __cplusplus
}
#endif

#endif /* GRAVITY_RESOURCE_TRACKER_H */
