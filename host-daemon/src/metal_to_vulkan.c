/*
 * metal_to_vulkan.c — Metal → Vulkan Translation Helpers
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include "metal_to_vulkan.h"

VkImageUsageFlags gravity_translate_texture_usage(uint32_t metal_usage,
                                                    gravity_pixel_format_t format)
{
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    /* Metal usage flags (from MTLTextureUsage) */
    #define MTL_TEXTURE_USAGE_SHADER_READ   0x01
    #define MTL_TEXTURE_USAGE_SHADER_WRITE  0x02
    #define MTL_TEXTURE_USAGE_RENDER_TARGET 0x04
    #define MTL_TEXTURE_USAGE_PIXEL_VIEW    0x10

    if (metal_usage & MTL_TEXTURE_USAGE_SHADER_READ) {
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (metal_usage & MTL_TEXTURE_USAGE_SHADER_WRITE) {
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (metal_usage & MTL_TEXTURE_USAGE_RENDER_TARGET) {
        if (gravity_pixel_format_is_depth_stencil(format)) {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }

    /* If no usage specified, assume general purpose */
    if (metal_usage == 0) {
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    return usage;
}

VkMemoryPropertyFlags gravity_translate_storage_mode(gravity_storage_mode_t mode)
{
    switch (mode) {
    case GRAVITY_STORAGE_SHARED:
        /* CPU + GPU visible (used for streaming data) */
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    case GRAVITY_STORAGE_MANAGED:
        /* CPU accessible but GPU can use efficiently.
         * On Vulkan, we use HOST_VISIBLE with caching. */
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    case GRAVITY_STORAGE_PRIVATE:
        /* GPU-only memory (fastest) */
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    case GRAVITY_STORAGE_MEMORYLESS:
        /* Transient render target — lazily allocated */
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
               VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

    default:
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
}

VkImageType gravity_translate_texture_type(gravity_texture_type_t type)
{
    switch (type) {
    case GRAVITY_TEXTURE_TYPE_1D:
    case GRAVITY_TEXTURE_TYPE_1D_ARRAY:
        return VK_IMAGE_TYPE_1D;

    case GRAVITY_TEXTURE_TYPE_2D:
    case GRAVITY_TEXTURE_TYPE_2D_ARRAY:
    case GRAVITY_TEXTURE_TYPE_2D_MS:
    case GRAVITY_TEXTURE_TYPE_CUBE:
    case GRAVITY_TEXTURE_TYPE_CUBE_ARRAY:
        return VK_IMAGE_TYPE_2D;

    case GRAVITY_TEXTURE_TYPE_3D:
        return VK_IMAGE_TYPE_3D;

    default:
        return VK_IMAGE_TYPE_2D;
    }
}

VkImageViewType gravity_translate_image_view_type(gravity_texture_type_t type)
{
    switch (type) {
    case GRAVITY_TEXTURE_TYPE_1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case GRAVITY_TEXTURE_TYPE_1D_ARRAY:
        return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case GRAVITY_TEXTURE_TYPE_2D:
    case GRAVITY_TEXTURE_TYPE_2D_MS:
        return VK_IMAGE_VIEW_TYPE_2D;
    case GRAVITY_TEXTURE_TYPE_2D_ARRAY:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case GRAVITY_TEXTURE_TYPE_CUBE:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case GRAVITY_TEXTURE_TYPE_CUBE_ARRAY:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    case GRAVITY_TEXTURE_TYPE_3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    default:
        return VK_IMAGE_VIEW_TYPE_2D;
    }
}

VkImageAspectFlags gravity_get_aspect_flags(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;

    case VK_FORMAT_S8_UINT:
        return VK_IMAGE_ASPECT_STENCIL_BIT;

    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}
