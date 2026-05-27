/*
 * gravity_formats.c — Metal ↔ Vulkan Format Mapping Implementation
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include "gravity_formats.h"

/* ═══════════════════════════════════════════════════════════════════════
 * Pixel Format Table
 * ═══════════════════════════════════════════════════════════════════════ */

static const gravity_format_entry_t pixel_format_table[] = {
    { GRAVITY_PIXEL_FORMAT_A8_UNORM,            GVK_FORMAT_A8_UNORM,                "A8Unorm",              1 },
    { GRAVITY_PIXEL_FORMAT_R8_UNORM,            GVK_FORMAT_R8_UNORM,                "R8Unorm",              1 },
    { GRAVITY_PIXEL_FORMAT_R8_SNORM,            GVK_FORMAT_R8_SNORM,                "R8Snorm",              1 },
    { GRAVITY_PIXEL_FORMAT_R8_UINT,             GVK_FORMAT_R8_UINT,                 "R8Uint",               1 },
    { GRAVITY_PIXEL_FORMAT_R16_UINT,            GVK_FORMAT_R16_UINT,                "R16Uint",              2 },
    { GRAVITY_PIXEL_FORMAT_R16_FLOAT,           GVK_FORMAT_R16_SFLOAT,              "R16Float",             2 },
    { GRAVITY_PIXEL_FORMAT_RG8_UNORM,           GVK_FORMAT_R8G8_UNORM,              "RG8Unorm",             2 },
    { GRAVITY_PIXEL_FORMAT_RG8_SNORM,           GVK_FORMAT_R8G8_SNORM,              "RG8Snorm",             2 },
    { GRAVITY_PIXEL_FORMAT_R32_UINT,            GVK_FORMAT_R32_UINT,                "R32Uint",              4 },
    { GRAVITY_PIXEL_FORMAT_R32_FLOAT,           GVK_FORMAT_R32_SFLOAT,              "R32Float",             4 },
    { GRAVITY_PIXEL_FORMAT_RG16_UINT,           GVK_FORMAT_R16G16_UINT,             "RG16Uint",             4 },
    { GRAVITY_PIXEL_FORMAT_RG16_FLOAT,          GVK_FORMAT_R16G16_SFLOAT,           "RG16Float",            4 },
    { GRAVITY_PIXEL_FORMAT_RGBA8_UNORM,         GVK_FORMAT_R8G8B8A8_UNORM,          "RGBA8Unorm",           4 },
    { GRAVITY_PIXEL_FORMAT_RGBA8_UNORM_SRGB,    GVK_FORMAT_R8G8B8A8_SRGB,           "RGBA8Unorm_sRGB",      4 },
    { GRAVITY_PIXEL_FORMAT_RGBA8_SNORM,         GVK_FORMAT_R8G8B8A8_SNORM,          "RGBA8Snorm",           4 },
    { GRAVITY_PIXEL_FORMAT_RGBA8_UINT,          GVK_FORMAT_R8G8B8A8_UINT,           "RGBA8Uint",            4 },
    { GRAVITY_PIXEL_FORMAT_BGRA8_UNORM,         GVK_FORMAT_B8G8R8A8_UNORM,          "BGRA8Unorm",           4 },
    { GRAVITY_PIXEL_FORMAT_BGRA8_UNORM_SRGB,    GVK_FORMAT_B8G8R8A8_SRGB,           "BGRA8Unorm_sRGB",      4 },
    { GRAVITY_PIXEL_FORMAT_RGB10A2_UNORM,       GVK_FORMAT_A2B10G10R10_UNORM_PACK32, "RGB10A2Unorm",         4 },
    { GRAVITY_PIXEL_FORMAT_RG32_UINT,           GVK_FORMAT_R32G32_UINT,             "RG32Uint",             8 },
    { GRAVITY_PIXEL_FORMAT_RG32_FLOAT,          GVK_FORMAT_R32G32_SFLOAT,           "RG32Float",            8 },
    { GRAVITY_PIXEL_FORMAT_RGBA16_UINT,         GVK_FORMAT_R16G16B16A16_UINT,       "RGBA16Uint",           8 },
    { GRAVITY_PIXEL_FORMAT_RGBA16_FLOAT,        GVK_FORMAT_R16G16B16A16_SFLOAT,     "RGBA16Float",          8 },
    { GRAVITY_PIXEL_FORMAT_RGBA32_UINT,         GVK_FORMAT_R32G32B32A32_UINT,       "RGBA32Uint",           16 },
    { GRAVITY_PIXEL_FORMAT_RGBA32_FLOAT,        GVK_FORMAT_R32G32B32A32_SFLOAT,     "RGBA32Float",          16 },
    { GRAVITY_PIXEL_FORMAT_DEPTH16_UNORM,       GVK_FORMAT_D16_UNORM,               "Depth16Unorm",         2 },
    { GRAVITY_PIXEL_FORMAT_DEPTH32_FLOAT,       GVK_FORMAT_D32_SFLOAT,              "Depth32Float",         4 },
    { GRAVITY_PIXEL_FORMAT_STENCIL8,            GVK_FORMAT_S8_UINT,                 "Stencil8",             1 },
    { GRAVITY_PIXEL_FORMAT_DEPTH24_STENCIL8,    GVK_FORMAT_D24_UNORM_S8_UINT,       "Depth24Unorm_Stencil8", 4 },
    { GRAVITY_PIXEL_FORMAT_DEPTH32_FLOAT_STENCIL8, GVK_FORMAT_D32_SFLOAT_S8_UINT,   "Depth32Float_Stencil8", 5 },
};

#define PIXEL_FORMAT_TABLE_SIZE (sizeof(pixel_format_table) / sizeof(pixel_format_table[0]))

const gravity_format_entry_t* gravity_get_pixel_format_table(uint32_t* count)
{
    if (count) *count = PIXEL_FORMAT_TABLE_SIZE;
    return pixel_format_table;
}

gravity_vk_format_t gravity_metal_to_vk_pixel_format(gravity_pixel_format_t metal_fmt)
{
    for (uint32_t i = 0; i < PIXEL_FORMAT_TABLE_SIZE; i++) {
        if (pixel_format_table[i].metal_format == metal_fmt) {
            return pixel_format_table[i].vk_format;
        }
    }
    return GVK_FORMAT_UNDEFINED;
}

uint32_t gravity_pixel_format_bpp(gravity_pixel_format_t fmt)
{
    for (uint32_t i = 0; i < PIXEL_FORMAT_TABLE_SIZE; i++) {
        if (pixel_format_table[i].metal_format == fmt) {
            return pixel_format_table[i].bytes_per_pixel;
        }
    }
    return 0;
}

int gravity_pixel_format_is_depth_stencil(gravity_pixel_format_t fmt)
{
    return (fmt >= GRAVITY_PIXEL_FORMAT_DEPTH16_UNORM &&
            fmt <= GRAVITY_PIXEL_FORMAT_DEPTH32_FLOAT_STENCIL8);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Vertex Format Mapping
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    gravity_vertex_format_t metal_fmt;
    gravity_vk_format_t     vk_fmt;
    uint32_t                size_bytes;
} vertex_format_entry_t;

static const vertex_format_entry_t vertex_format_table[] = {
    { GRAVITY_VERTEX_FORMAT_FLOAT,          GVK_FORMAT_R32_SFLOAT_V,         4 },
    { GRAVITY_VERTEX_FORMAT_FLOAT2,         GVK_FORMAT_R32G32_SFLOAT_V,      8 },
    { GRAVITY_VERTEX_FORMAT_FLOAT3,         GVK_FORMAT_R32G32B32_SFLOAT,     12 },
    { GRAVITY_VERTEX_FORMAT_FLOAT4,         GVK_FORMAT_R32G32B32A32_SFLOAT_V, 16 },
    { GRAVITY_VERTEX_FORMAT_UCHAR4,         GVK_FORMAT_R8G8B8A8_UINT,        4 },
    { GRAVITY_VERTEX_FORMAT_UCHAR4_NORM,    GVK_FORMAT_R8G8B8A8_UNORM_V,     4 },
    { GRAVITY_VERTEX_FORMAT_SHORT2,         GVK_FORMAT_R16G16_SINT,          4 },
    { GRAVITY_VERTEX_FORMAT_SHORT4,         GVK_FORMAT_R16G16B16A16_SINT,    8 },
    { GRAVITY_VERTEX_FORMAT_HALF2,          GVK_FORMAT_R16G16_SFLOAT_V,      4 },
    { GRAVITY_VERTEX_FORMAT_HALF4,          GVK_FORMAT_R16G16B16A16_SFLOAT_V, 8 },
    { GRAVITY_VERTEX_FORMAT_INT,            GVK_FORMAT_R32_SINT,             4 },
    { GRAVITY_VERTEX_FORMAT_INT2,           GVK_FORMAT_R32G32_SINT,          8 },
    { GRAVITY_VERTEX_FORMAT_INT3,           GVK_FORMAT_R32G32B32_SINT,       12 },
    { GRAVITY_VERTEX_FORMAT_INT4,           GVK_FORMAT_R32G32B32A32_SINT,    16 },
    { GRAVITY_VERTEX_FORMAT_UINT,           GVK_FORMAT_R32_UINT_V,           4 },
};

#define VERTEX_FORMAT_TABLE_SIZE (sizeof(vertex_format_table) / sizeof(vertex_format_table[0]))

gravity_vk_format_t gravity_metal_to_vk_vertex_format(gravity_vertex_format_t metal_fmt)
{
    for (uint32_t i = 0; i < VERTEX_FORMAT_TABLE_SIZE; i++) {
        if (vertex_format_table[i].metal_fmt == metal_fmt) {
            return vertex_format_table[i].vk_fmt;
        }
    }
    return GVK_FORMAT_UNDEFINED;
}

uint32_t gravity_vertex_format_size(gravity_vertex_format_t fmt)
{
    for (uint32_t i = 0; i < VERTEX_FORMAT_TABLE_SIZE; i++) {
        if (vertex_format_table[i].metal_fmt == fmt) {
            return vertex_format_table[i].size_bytes;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Enum Conversion Functions
 * Values match VkXxx enum values directly.
 * ═══════════════════════════════════════════════════════════════════════ */

uint32_t gravity_to_vk_primitive_topology(gravity_primitive_type_t prim)
{
    switch (prim) {
    case GRAVITY_PRIMITIVE_POINT:            return 0;  /* VK_PRIMITIVE_TOPOLOGY_POINT_LIST */
    case GRAVITY_PRIMITIVE_LINE:             return 1;  /* VK_PRIMITIVE_TOPOLOGY_LINE_LIST */
    case GRAVITY_PRIMITIVE_LINE_STRIP:       return 2;  /* VK_PRIMITIVE_TOPOLOGY_LINE_STRIP */
    case GRAVITY_PRIMITIVE_TRIANGLE:         return 3;  /* VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST */
    case GRAVITY_PRIMITIVE_TRIANGLE_STRIP:   return 4;  /* VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP */
    default:                                return 3;
    }
}

uint32_t gravity_to_vk_index_type(gravity_index_type_t idx)
{
    switch (idx) {
    case GRAVITY_INDEX_UINT16:  return 0;  /* VK_INDEX_TYPE_UINT16 */
    case GRAVITY_INDEX_UINT32:  return 1;  /* VK_INDEX_TYPE_UINT32 */
    default:                   return 1;
    }
}

uint32_t gravity_to_vk_cull_mode(gravity_cull_mode_t mode)
{
    switch (mode) {
    case GRAVITY_CULL_NONE:     return 0;  /* VK_CULL_MODE_NONE */
    case GRAVITY_CULL_FRONT:    return 1;  /* VK_CULL_MODE_FRONT_BIT */
    case GRAVITY_CULL_BACK:     return 2;  /* VK_CULL_MODE_BACK_BIT */
    default:                   return 0;
    }
}

uint32_t gravity_to_vk_front_face(gravity_winding_t winding)
{
    /* Metal CW = Vulkan CW is VK_FRONT_FACE_CLOCKWISE = 1
     * Metal CCW = Vulkan CCW is VK_FRONT_FACE_COUNTER_CLOCKWISE = 0
     * But Metal's default Y axis is flipped vs Vulkan, so we invert. */
    switch (winding) {
    case GRAVITY_WINDING_CW:    return 0;  /* VK_FRONT_FACE_COUNTER_CLOCKWISE (inverted) */
    case GRAVITY_WINDING_CCW:   return 1;  /* VK_FRONT_FACE_CLOCKWISE (inverted) */
    default:                   return 0;
    }
}

uint32_t gravity_to_vk_compare_op(gravity_compare_function_t func)
{
    /* VkCompareOp and MTLCompareFunction values align 1:1 */
    return (uint32_t)func;
}

uint32_t gravity_to_vk_blend_factor(gravity_blend_factor_t factor)
{
    /* VkBlendFactor values */
    switch (factor) {
    case GRAVITY_BLEND_ZERO:                    return 0;
    case GRAVITY_BLEND_ONE:                     return 1;
    case GRAVITY_BLEND_SRC_COLOR:               return 2;
    case GRAVITY_BLEND_ONE_MINUS_SRC_COLOR:     return 3;
    case GRAVITY_BLEND_DST_COLOR:               return 4;
    case GRAVITY_BLEND_ONE_MINUS_DST_COLOR:     return 5;
    case GRAVITY_BLEND_SRC_ALPHA:               return 6;
    case GRAVITY_BLEND_ONE_MINUS_SRC_ALPHA:     return 7;
    case GRAVITY_BLEND_DST_ALPHA:               return 8;
    case GRAVITY_BLEND_ONE_MINUS_DST_ALPHA:     return 9;
    case GRAVITY_BLEND_BLEND_COLOR:             return 12; /* VK_BLEND_FACTOR_CONSTANT_COLOR */
    case GRAVITY_BLEND_ONE_MINUS_BLEND_COLOR:   return 13;
    case GRAVITY_BLEND_BLEND_ALPHA:             return 14; /* VK_BLEND_FACTOR_CONSTANT_ALPHA */
    case GRAVITY_BLEND_ONE_MINUS_BLEND_ALPHA:   return 15;
    case GRAVITY_BLEND_SRC_ALPHA_SATURATED:     return 10; /* VK_BLEND_FACTOR_SRC_ALPHA_SATURATE */
    default:                                   return 0;
    }
}

uint32_t gravity_to_vk_blend_op(gravity_blend_operation_t op)
{
    /* VkBlendOp values align 1:1 with Metal */
    return (uint32_t)op;
}

uint32_t gravity_to_vk_stencil_op(gravity_stencil_operation_t op)
{
    /* VkStencilOp values align 1:1 with Metal */
    return (uint32_t)op;
}

uint32_t gravity_to_vk_load_op(gravity_load_action_t action)
{
    switch (action) {
    case GRAVITY_LOAD_LOAD:         return 0;  /* VK_ATTACHMENT_LOAD_OP_LOAD */
    case GRAVITY_LOAD_CLEAR:        return 1;  /* VK_ATTACHMENT_LOAD_OP_CLEAR */
    case GRAVITY_LOAD_DONT_CARE:    return 2;  /* VK_ATTACHMENT_LOAD_OP_DONT_CARE */
    default:                       return 2;
    }
}

uint32_t gravity_to_vk_store_op(gravity_store_action_t action)
{
    switch (action) {
    case GRAVITY_STORE_STORE:       return 0;  /* VK_ATTACHMENT_STORE_OP_STORE */
    case GRAVITY_STORE_DONT_CARE:   return 1;  /* VK_ATTACHMENT_STORE_OP_DONT_CARE */
    default:                       return 0;   /* Default to store */
    }
}

uint32_t gravity_to_vk_address_mode(gravity_sampler_address_mode_t mode)
{
    switch (mode) {
    case GRAVITY_ADDRESS_REPEAT:            return 0;  /* VK_SAMPLER_ADDRESS_MODE_REPEAT */
    case GRAVITY_ADDRESS_MIRROR_REPEAT:     return 1;  /* VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT */
    case GRAVITY_ADDRESS_CLAMP_TO_EDGE:     return 2;  /* VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE */
    case GRAVITY_ADDRESS_CLAMP_TO_BORDER:   return 3;  /* VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER */
    case GRAVITY_ADDRESS_MIRROR_CLAMP:      return 4;  /* VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE */
    case GRAVITY_ADDRESS_CLAMP_TO_ZERO:     return 3;  /* Map to CLAMP_TO_BORDER with zero border color */
    default:                               return 2;
    }
}

uint32_t gravity_to_vk_filter(gravity_sampler_min_mag_filter_t filter)
{
    switch (filter) {
    case GRAVITY_FILTER_NEAREST:    return 0;  /* VK_FILTER_NEAREST */
    case GRAVITY_FILTER_LINEAR:     return 1;  /* VK_FILTER_LINEAR */
    default:                       return 0;
    }
}

uint32_t gravity_to_vk_mipmap_mode(gravity_sampler_mip_filter_t filter)
{
    switch (filter) {
    case GRAVITY_MIP_NOT_MIPMAPPED: return 0;  /* VK_SAMPLER_MIPMAP_MODE_NEAREST (effectively disabled) */
    case GRAVITY_MIP_NEAREST:       return 0;  /* VK_SAMPLER_MIPMAP_MODE_NEAREST */
    case GRAVITY_MIP_LINEAR:        return 1;  /* VK_SAMPLER_MIPMAP_MODE_LINEAR */
    default:                       return 0;
    }
}

uint32_t gravity_to_vk_polygon_mode(gravity_triangle_fill_mode_t mode)
{
    switch (mode) {
    case GRAVITY_FILL_FILL:     return 0;  /* VK_POLYGON_MODE_FILL */
    case GRAVITY_FILL_LINES:    return 1;  /* VK_POLYGON_MODE_LINE */
    default:                   return 0;
    }
}
