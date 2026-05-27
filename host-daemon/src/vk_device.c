/*
 * vk_device.c — Vulkan Device Management Implementation
 *
 * Initializes Vulkan targeting the Intel UHD 770 iGPU via Mesa ANV.
 * Creates instance, selects physical device, creates logical device
 * with graphics/compute/transfer queues.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include "vk_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Debug Callback
 * ═══════════════════════════════════════════════════════════════════════ */

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* user_data)
{
    (void)type;
    (void)user_data;

    const char* level = "INFO";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        level = "ERROR";
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        level = "WARN";
    }

    fprintf(stderr, "[Vulkan %s] %s\n", level, data->pMessage);
    return VK_FALSE;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Instance Creation
 * ═══════════════════════════════════════════════════════════════════════ */

static int create_instance(gravity_vk_state_t* state, bool validation)
{
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "GravityGPU Daemon",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "GravityGPU",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    const char* validation_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    const char* extensions[] = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };

    if (validation) {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = validation_layers;
        create_info.enabledExtensionCount = 1;
        create_info.ppEnabledExtensionNames = extensions;
    }

    VkResult result = vkCreateInstance(&create_info, NULL, &state->instance);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "[vk_device] Failed to create Vulkan instance: %d\n", result);
        return -1;
    }

    /* Set up debug messenger if validation is enabled */
    if (validation) {
        PFN_vkCreateDebugUtilsMessengerEXT createDebugMessenger =
            (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(state->instance, "vkCreateDebugUtilsMessengerEXT");

        if (createDebugMessenger) {
            VkDebugUtilsMessengerCreateInfoEXT dbg_info = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = debug_callback,
            };
            createDebugMessenger(state->instance, &dbg_info, NULL,
                                 &state->debug_messenger);
        }
    }

    state->validation_enabled = validation;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Physical Device Selection
 * ═══════════════════════════════════════════════════════════════════════ */

static int select_physical_device(gravity_vk_state_t* state)
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(state->instance, &device_count, NULL);

    if (device_count == 0) {
        fprintf(stderr, "[vk_device] No Vulkan-capable GPUs found!\n");
        return -1;
    }

    VkPhysicalDevice* devices = calloc(device_count, sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(state->instance, &device_count, devices);

    /* Prefer Intel integrated GPU (for our use case), fall back to any GPU */
    VkPhysicalDevice selected = VK_NULL_HANDLE;
    VkPhysicalDevice fallback = VK_NULL_HANDLE;

    printf("[vk_device] Found %u GPU(s):\n", device_count);

    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);

        const char* type_str = "Unknown";
        switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: type_str = "Integrated"; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   type_str = "Discrete";   break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    type_str = "Virtual";    break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            type_str = "CPU";        break;
        default: break;
        }

        printf("  [%u] %s (%s, Vulkan %u.%u)\n", i, props.deviceName, type_str,
               VK_VERSION_MAJOR(props.apiVersion),
               VK_VERSION_MINOR(props.apiVersion));

        /* Prefer Intel integrated (for development/target HW) */
        if (props.vendorID == 0x8086 &&
            props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            selected = devices[i];
        }

        if (!fallback) {
            fallback = devices[i];
        }
    }

    if (!selected) {
        selected = fallback;
    }

    state->physical_device = selected;
    vkGetPhysicalDeviceProperties(selected, &state->properties);
    vkGetPhysicalDeviceMemoryProperties(selected, &state->memory_properties);
    vkGetPhysicalDeviceFeatures(selected, &state->features);

    strncpy(state->device_name, state->properties.deviceName,
            sizeof(state->device_name) - 1);
    state->device_name[sizeof(state->device_name) - 1] = '\0';

    printf("[vk_device] Selected: %s\n", state->device_name);

    free(devices);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Queue Family Selection
 * ═══════════════════════════════════════════════════════════════════════ */

static int find_queue_families(gravity_vk_state_t* state)
{
    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(state->physical_device,
                                              &family_count, NULL);

    VkQueueFamilyProperties* families = calloc(family_count,
                                                sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(state->physical_device,
                                              &family_count, families);

    state->graphics_queue_family = UINT32_MAX;
    state->compute_queue_family  = UINT32_MAX;
    state->transfer_queue_family = UINT32_MAX;

    for (uint32_t i = 0; i < family_count; i++) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            state->graphics_queue_family == UINT32_MAX) {
            state->graphics_queue_family = i;
        }

        /* Prefer a dedicated compute queue */
        if ((families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            state->compute_queue_family == UINT32_MAX) {
            state->compute_queue_family = i;
        }

        /* Prefer a dedicated transfer queue */
        if ((families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            state->transfer_queue_family == UINT32_MAX) {
            state->transfer_queue_family = i;
        }
    }

    /* Fall back to graphics queue if no dedicated compute/transfer */
    if (state->compute_queue_family == UINT32_MAX) {
        state->compute_queue_family = state->graphics_queue_family;
    }
    if (state->transfer_queue_family == UINT32_MAX) {
        state->transfer_queue_family = state->graphics_queue_family;
    }

    free(families);

    if (state->graphics_queue_family == UINT32_MAX) {
        fprintf(stderr, "[vk_device] No graphics queue family found!\n");
        return -1;
    }

    printf("[vk_device] Queue families: graphics=%u, compute=%u, transfer=%u\n",
           state->graphics_queue_family, state->compute_queue_family,
           state->transfer_queue_family);

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Logical Device Creation
 * ═══════════════════════════════════════════════════════════════════════ */

static int create_logical_device(gravity_vk_state_t* state)
{
    /* Collect unique queue families */
    uint32_t unique_families[3];
    uint32_t unique_count = 0;

    unique_families[unique_count++] = state->graphics_queue_family;

    if (state->compute_queue_family != state->graphics_queue_family) {
        unique_families[unique_count++] = state->compute_queue_family;
    }
    if (state->transfer_queue_family != state->graphics_queue_family &&
        state->transfer_queue_family != state->compute_queue_family) {
        unique_families[unique_count++] = state->transfer_queue_family;
    }

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_infos[3];

    for (uint32_t i = 0; i < unique_count; i++) {
        queue_infos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_families[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
    }

    /* Enable features needed for Metal translation */
    VkPhysicalDeviceFeatures enabled_features = {
        .fillModeNonSolid = VK_TRUE,       /* Metal wireframe rendering */
        .wideLines = VK_FALSE,             /* Not available on Intel */
        .samplerAnisotropy = VK_TRUE,
        .multiDrawIndirect = VK_TRUE,
        .depthClamp = VK_TRUE,
        .depthBiasClamp = VK_TRUE,
        .independentBlend = VK_TRUE,
        .fragmentStoresAndAtomics = VK_TRUE,
        .vertexPipelineStoresAndAtomics = VK_TRUE,
    };

    /* Vulkan 1.3 features: dynamic rendering (avoids VkRenderPass objects) */
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = VK_TRUE,
    };

    /* Vulkan 1.3 features: synchronization2 */
    VkPhysicalDeviceSynchronization2Features sync2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .pNext = &dynamic_rendering,
        .synchronization2 = VK_TRUE,
    };

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &sync2,
        .queueCreateInfoCount = unique_count,
        .pQueueCreateInfos = queue_infos,
        .pEnabledFeatures = &enabled_features,
    };

    VkResult result = vkCreateDevice(state->physical_device, &device_info,
                                      NULL, &state->device);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "[vk_device] Failed to create logical device: %d\n", result);
        return -1;
    }

    /* Get queue handles */
    vkGetDeviceQueue(state->device, state->graphics_queue_family, 0,
                     &state->graphics_queue);
    vkGetDeviceQueue(state->device, state->compute_queue_family, 0,
                     &state->compute_queue);
    vkGetDeviceQueue(state->device, state->transfer_queue_family, 0,
                     &state->transfer_queue);

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Command Pools
 * ═══════════════════════════════════════════════════════════════════════ */

static int create_command_pools(gravity_vk_state_t* state)
{
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };

    /* Graphics pool */
    pool_info.queueFamilyIndex = state->graphics_queue_family;
    if (vkCreateCommandPool(state->device, &pool_info, NULL,
                             &state->graphics_cmd_pool) != VK_SUCCESS) {
        fprintf(stderr, "[vk_device] Failed to create graphics command pool\n");
        return -1;
    }

    /* Compute pool (may be same queue family) */
    pool_info.queueFamilyIndex = state->compute_queue_family;
    if (vkCreateCommandPool(state->device, &pool_info, NULL,
                             &state->compute_cmd_pool) != VK_SUCCESS) {
        fprintf(stderr, "[vk_device] Failed to create compute command pool\n");
        return -1;
    }

    /* Transfer pool */
    pool_info.queueFamilyIndex = state->transfer_queue_family;
    if (vkCreateCommandPool(state->device, &pool_info, NULL,
                             &state->transfer_cmd_pool) != VK_SUCCESS) {
        fprintf(stderr, "[vk_device] Failed to create transfer command pool\n");
        return -1;
    }

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Pipeline Cache
 * ═══════════════════════════════════════════════════════════════════════ */

static int create_pipeline_cache(gravity_vk_state_t* state)
{
    VkPipelineCacheCreateInfo cache_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
    };

    /* TODO: Load cached data from disk */

    if (vkCreatePipelineCache(state->device, &cache_info, NULL,
                               &state->pipeline_cache) != VK_SUCCESS) {
        fprintf(stderr, "[vk_device] Warning: Failed to create pipeline cache\n");
        state->pipeline_cache = VK_NULL_HANDLE;
    }

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Descriptor Pool
 * ═══════════════════════════════════════════════════════════════════════ */

static int create_descriptor_pool(gravity_vk_state_t* state)
{
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1024 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1024 },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                256 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          256 },
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 4096,
        .poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]),
        .pPoolSizes = pool_sizes,
    };

    if (vkCreateDescriptorPool(state->device, &pool_info, NULL,
                                &state->descriptor_pool) != VK_SUCCESS) {
        fprintf(stderr, "[vk_device] Warning: Failed to create descriptor pool\n");
        state->descriptor_pool = VK_NULL_HANDLE;
    }

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════ */

int gravity_vk_init(gravity_vk_state_t* state, bool validation)
{
    memset(state, 0, sizeof(*state));

    printf("[vk_device] Initializing Vulkan 1.3...\n");

    if (create_instance(state, validation) < 0) return -1;
    if (select_physical_device(state) < 0) return -1;
    if (find_queue_families(state) < 0) return -1;
    if (create_logical_device(state) < 0) return -1;
    if (create_command_pools(state) < 0) return -1;
    if (create_pipeline_cache(state) < 0) return -1;
    if (create_descriptor_pool(state) < 0) return -1;

    state->active_cmd_buf = VK_NULL_HANDLE;
    state->in_render_pass = false;

    printf("[vk_device] Vulkan initialization complete.\n");
    printf("[vk_device]   Device: %s\n", state->device_name);
    printf("[vk_device]   API:    Vulkan %u.%u.%u\n",
           VK_VERSION_MAJOR(state->properties.apiVersion),
           VK_VERSION_MINOR(state->properties.apiVersion),
           VK_VERSION_PATCH(state->properties.apiVersion));
    printf("[vk_device]   Max texture size: %u\n",
           state->properties.limits.maxImageDimension2D);

    return 0;
}

void gravity_vk_destroy(gravity_vk_state_t* state)
{
    if (!state || !state->device) return;

    vkDeviceWaitIdle(state->device);

    if (state->descriptor_pool) {
        vkDestroyDescriptorPool(state->device, state->descriptor_pool, NULL);
    }
    if (state->pipeline_cache) {
        /* TODO: Save pipeline cache to disk before destroying */
        vkDestroyPipelineCache(state->device, state->pipeline_cache, NULL);
    }
    if (state->graphics_cmd_pool) {
        vkDestroyCommandPool(state->device, state->graphics_cmd_pool, NULL);
    }
    if (state->compute_cmd_pool &&
        state->compute_cmd_pool != state->graphics_cmd_pool) {
        vkDestroyCommandPool(state->device, state->compute_cmd_pool, NULL);
    }
    if (state->transfer_cmd_pool &&
        state->transfer_cmd_pool != state->graphics_cmd_pool &&
        state->transfer_cmd_pool != state->compute_cmd_pool) {
        vkDestroyCommandPool(state->device, state->transfer_cmd_pool, NULL);
    }

    vkDestroyDevice(state->device, NULL);

    if (state->debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugMessenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(state->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroyDebugMessenger) {
            destroyDebugMessenger(state->instance, state->debug_messenger, NULL);
        }
    }

    vkDestroyInstance(state->instance, NULL);
    memset(state, 0, sizeof(*state));
}

uint32_t gravity_vk_find_memory_type(const gravity_vk_state_t* state,
                                      uint32_t type_filter,
                                      VkMemoryPropertyFlags properties)
{
    for (uint32_t i = 0; i < state->memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (state->memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

VkCommandBuffer gravity_vk_begin_single_time_commands(gravity_vk_state_t* state)
{
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = state->graphics_cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd_buf;
    vkAllocateCommandBuffers(state->device, &alloc_info, &cmd_buf);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd_buf, &begin_info);

    return cmd_buf;
}

void gravity_vk_end_single_time_commands(gravity_vk_state_t* state,
                                          VkCommandBuffer cmd_buf)
{
    vkEndCommandBuffer(cmd_buf);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buf,
    };

    vkQueueSubmit(state->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(state->graphics_queue);

    vkFreeCommandBuffers(state->device, state->graphics_cmd_pool, 1, &cmd_buf);
}
