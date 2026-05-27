/*
 * metal_to_vulkan.h — Metal → Vulkan Translation Engine Header
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#ifndef GRAVITY_METAL_TO_VULKAN_H
#define GRAVITY_METAL_TO_VULKAN_H

#include "vk_device.h"
#include "resource_tracker.h"
#include "../../protocol/include/gravity_protocol.h"
#include "../../protocol/include/gravity_formats.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Included by cmd_processor.h — no additional declarations needed here.
 * This header exists as a namespace for the translation helper functions
 * that will expand significantly in Phase 2+. */

/**
 * Translate Metal texture usage flags to Vulkan image usage flags.
 */
VkImageUsageFlags gravity_translate_texture_usage(uint32_t metal_usage,
                                                    gravity_pixel_format_t format);

/**
 * Translate Metal storage mode to Vulkan memory property flags.
 */
VkMemoryPropertyFlags gravity_translate_storage_mode(gravity_storage_mode_t mode);

/**
 * Translate Metal texture type to Vulkan image type.
 */
VkImageType gravity_translate_texture_type(gravity_texture_type_t type);

/**
 * Translate Metal texture type to Vulkan image view type.
 */
VkImageViewType gravity_translate_image_view_type(gravity_texture_type_t type);

/**
 * Get appropriate VkImageAspectFlags for a pixel format.
 */
VkImageAspectFlags gravity_get_aspect_flags(VkFormat format);

#ifdef __cplusplus
}
#endif

#endif /* GRAVITY_METAL_TO_VULKAN_H */
