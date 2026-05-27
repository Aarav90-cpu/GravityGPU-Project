/*
 * gravity_formats.h — Metal ↔ Vulkan Format Mapping Tables
 *
 * Maps Metal pixel formats, vertex formats, and blend factors to their
 * Vulkan equivalents. Used by the host translation daemon.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#ifndef GRAVITY_FORMATS_H
#define GRAVITY_FORMATS_H

#include "gravity_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════
 * Vulkan Format Constants (mirrors VkFormat values)
 * We define our own to avoid requiring vulkan headers in shared code.
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum gravity_vk_format {
    GVK_FORMAT_UNDEFINED                = 0,
    GVK_FORMAT_R8_UNORM                 = 9,
    GVK_FORMAT_R8_SNORM                 = 10,
    GVK_FORMAT_R8_UINT                  = 13,
    GVK_FORMAT_R16_UINT                 = 74,
    GVK_FORMAT_R16_SFLOAT               = 76,
    GVK_FORMAT_R8G8_UNORM               = 16,
    GVK_FORMAT_R8G8_SNORM               = 17,
    GVK_FORMAT_R32_UINT                 = 98,
    GVK_FORMAT_R32_SFLOAT               = 100,
    GVK_FORMAT_R16G16_UINT              = 81,
    GVK_FORMAT_R16G16_SFLOAT            = 83,
    GVK_FORMAT_R8G8B8A8_UNORM           = 37,
    GVK_FORMAT_R8G8B8A8_SRGB            = 43,
    GVK_FORMAT_R8G8B8A8_SNORM           = 38,
    GVK_FORMAT_R8G8B8A8_UINT            = 41,
    GVK_FORMAT_B8G8R8A8_UNORM           = 44,
    GVK_FORMAT_B8G8R8A8_SRGB            = 50,
    GVK_FORMAT_A2B10G10R10_UNORM_PACK32 = 64,
    GVK_FORMAT_R32G32_UINT              = 101,
    GVK_FORMAT_R32G32_SFLOAT            = 103,
    GVK_FORMAT_R16G16B16A16_UINT        = 95,
    GVK_FORMAT_R16G16B16A16_SFLOAT      = 97,
    GVK_FORMAT_R32G32B32A32_UINT        = 107,
    GVK_FORMAT_R32G32B32A32_SFLOAT      = 109,
    GVK_FORMAT_D16_UNORM                = 124,
    GVK_FORMAT_D32_SFLOAT               = 126,
    GVK_FORMAT_S8_UINT                  = 127,
    GVK_FORMAT_D24_UNORM_S8_UINT        = 129,
    GVK_FORMAT_D32_SFLOAT_S8_UINT       = 130,
    GVK_FORMAT_A8_UNORM                 = 1000470000,  /* VK_KHR_maintenance5 */

    /* Vertex formats */
    GVK_FORMAT_R32_SFLOAT_V             = 100,
    GVK_FORMAT_R32G32_SFLOAT_V          = 103,
    GVK_FORMAT_R32G32B32_SFLOAT         = 106,
    GVK_FORMAT_R32G32B32A32_SFLOAT_V    = 109,
    GVK_FORMAT_R8G8B8A8_UNORM_V         = 37,
    GVK_FORMAT_R16G16_SINT              = 82,
    GVK_FORMAT_R16G16B16A16_SINT        = 96,
    GVK_FORMAT_R16G16_SFLOAT_V          = 83,
    GVK_FORMAT_R16G16B16A16_SFLOAT_V    = 97,
    GVK_FORMAT_R32_SINT                 = 99,
    GVK_FORMAT_R32G32_SINT              = 102,
    GVK_FORMAT_R32G32B32_SINT           = 105,
    GVK_FORMAT_R32G32B32A32_SINT        = 108,
    GVK_FORMAT_R32_UINT_V               = 98,
} gravity_vk_format_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Pixel Format Mapping
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_format_entry {
    gravity_pixel_format_t  metal_format;
    gravity_vk_format_t     vk_format;
    const char*             name;
    uint32_t                bytes_per_pixel;
} gravity_format_entry_t;

/* Number of entries in the pixel format table */
#define GRAVITY_PIXEL_FORMAT_TABLE_SIZE 30

/**
 * Get the full pixel format mapping table.
 * Returns pointer to static array, sets *count to number of entries.
 */
const gravity_format_entry_t* gravity_get_pixel_format_table(uint32_t* count);

/**
 * Convert a Metal pixel format to its Vulkan equivalent.
 * Returns GVK_FORMAT_UNDEFINED if not found.
 */
gravity_vk_format_t gravity_metal_to_vk_pixel_format(gravity_pixel_format_t metal_fmt);

/**
 * Get bytes-per-pixel for a Metal pixel format.
 * Returns 0 if unknown.
 */
uint32_t gravity_pixel_format_bpp(gravity_pixel_format_t fmt);

/**
 * Check if a pixel format is a depth/stencil format.
 */
int gravity_pixel_format_is_depth_stencil(gravity_pixel_format_t fmt);

/* ═══════════════════════════════════════════════════════════════════════
 * Vertex Format Mapping
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Convert a Metal vertex format to its Vulkan equivalent.
 * Returns GVK_FORMAT_UNDEFINED if not found.
 */
gravity_vk_format_t gravity_metal_to_vk_vertex_format(gravity_vertex_format_t metal_fmt);

/**
 * Get size in bytes of a vertex format.
 */
uint32_t gravity_vertex_format_size(gravity_vertex_format_t fmt);

/* ═══════════════════════════════════════════════════════════════════════
 * Enum Conversions (used on host side to map to Vulkan enums)
 *
 * These return the raw uint32_t value matching the VkXxx enum.
 * ═══════════════════════════════════════════════════════════════════════ */

/** Map gravity_primitive_type_t → VkPrimitiveTopology */
uint32_t gravity_to_vk_primitive_topology(gravity_primitive_type_t prim);

/** Map gravity_index_type_t → VkIndexType */
uint32_t gravity_to_vk_index_type(gravity_index_type_t idx);

/** Map gravity_cull_mode_t → VkCullModeFlags */
uint32_t gravity_to_vk_cull_mode(gravity_cull_mode_t mode);

/** Map gravity_winding_t → VkFrontFace */
uint32_t gravity_to_vk_front_face(gravity_winding_t winding);

/** Map gravity_compare_function_t → VkCompareOp */
uint32_t gravity_to_vk_compare_op(gravity_compare_function_t func);

/** Map gravity_blend_factor_t → VkBlendFactor */
uint32_t gravity_to_vk_blend_factor(gravity_blend_factor_t factor);

/** Map gravity_blend_operation_t → VkBlendOp */
uint32_t gravity_to_vk_blend_op(gravity_blend_operation_t op);

/** Map gravity_stencil_operation_t → VkStencilOp */
uint32_t gravity_to_vk_stencil_op(gravity_stencil_operation_t op);

/** Map gravity_load_action_t → VkAttachmentLoadOp */
uint32_t gravity_to_vk_load_op(gravity_load_action_t action);

/** Map gravity_store_action_t → VkAttachmentStoreOp */
uint32_t gravity_to_vk_store_op(gravity_store_action_t action);

/** Map gravity_sampler_address_mode_t → VkSamplerAddressMode */
uint32_t gravity_to_vk_address_mode(gravity_sampler_address_mode_t mode);

/** Map gravity_sampler_min_mag_filter_t → VkFilter */
uint32_t gravity_to_vk_filter(gravity_sampler_min_mag_filter_t filter);

/** Map gravity_sampler_mip_filter_t → VkSamplerMipmapMode */
uint32_t gravity_to_vk_mipmap_mode(gravity_sampler_mip_filter_t filter);

/** Map gravity_triangle_fill_mode_t → VkPolygonMode */
uint32_t gravity_to_vk_polygon_mode(gravity_triangle_fill_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* GRAVITY_FORMATS_H */
