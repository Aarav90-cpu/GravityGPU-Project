/*
 * vk_device.h — Vulkan Device Management
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#ifndef GRAVITY_VK_DEVICE_H
#define GRAVITY_VK_DEVICE_H

#include <vulkan/vulkan.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════
 * Vulkan State
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_vk_state {
    VkInstance          instance;
    VkPhysicalDevice    physical_device;
    VkDevice            device;

    /* Queues */
    VkQueue             graphics_queue;
    VkQueue             compute_queue;
    VkQueue             transfer_queue;
    uint32_t            graphics_queue_family;
    uint32_t            compute_queue_family;
    uint32_t            transfer_queue_family;

    /* Command pools (one per queue family) */
    VkCommandPool       graphics_cmd_pool;
    VkCommandPool       compute_cmd_pool;
    VkCommandPool       transfer_cmd_pool;

    /* Device properties */
    char                device_name[256];
    VkPhysicalDeviceProperties      properties;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkPhysicalDeviceFeatures         features;

    /* Validation */
    VkDebugUtilsMessengerEXT debug_messenger;
    bool                     validation_enabled;

    /* Active command buffer being recorded */
    VkCommandBuffer     active_cmd_buf;
    bool                in_render_pass;

    /* Pipeline cache */
    VkPipelineCache     pipeline_cache;

    /* Descriptor pool */
    VkDescriptorPool    descriptor_pool;

} gravity_vk_state_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Initialize Vulkan: create instance, select physical device, create
 * logical device with queues and command pools.
 *
 * @param state     Output Vulkan state
 * @param validation Enable validation layers
 * @return 0 on success, -1 on failure
 */
int gravity_vk_init(gravity_vk_state_t* state, bool validation);

/**
 * Destroy all Vulkan objects and free resources.
 */
void gravity_vk_destroy(gravity_vk_state_t* state);

/**
 * Find a memory type index suitable for the given requirements.
 *
 * @param state         Vulkan state
 * @param type_filter   Bitmask of allowed memory types (from VkMemoryRequirements)
 * @param properties    Required memory property flags
 * @return Memory type index, or UINT32_MAX if not found
 */
uint32_t gravity_vk_find_memory_type(const gravity_vk_state_t* state,
                                      uint32_t type_filter,
                                      VkMemoryPropertyFlags properties);

/**
 * Allocate a one-shot command buffer from the graphics pool.
 */
VkCommandBuffer gravity_vk_begin_single_time_commands(gravity_vk_state_t* state);

/**
 * End and submit a one-shot command buffer, wait for completion.
 */
void gravity_vk_end_single_time_commands(gravity_vk_state_t* state,
                                          VkCommandBuffer cmd_buf);

#ifdef __cplusplus
}
#endif

#endif /* GRAVITY_VK_DEVICE_H */
