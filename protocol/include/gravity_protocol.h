/*
 * gravity_protocol.h — GravityGPU Wire Protocol
 *
 * Defines the serialization format for Metal GPU commands crossing
 * the guest (macOS) ↔ host (Linux) boundary via shared memory.
 *
 * Both the guest driver (GravityGPU.kext) and the host daemon (gravityd)
 * include this header. All structs are packed and use fixed-width types
 * to ensure identical layout across compilers.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#ifndef GRAVITY_PROTOCOL_H
#define GRAVITY_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════
 * Protocol Version
 * ═══════════════════════════════════════════════════════════════════════ */

#define GRAVITY_PROTOCOL_VERSION_MAJOR  0
#define GRAVITY_PROTOCOL_VERSION_MINOR  1
#define GRAVITY_PROTOCOL_MAGIC          0x47525659  /* "GRVY" */

/* ═══════════════════════════════════════════════════════════════════════
 * BAR0 Register Map (MMIO Control Registers)
 * ═══════════════════════════════════════════════════════════════════════ */

#define GRAVITY_REG_MAGIC 0x000 /* R:  Always reads 0x47525659 ("GRVY") */
#define GRAVITY_REG_VERSION 0x004 /* R:  Protocol version (major << 16 | minor) */
#define GRAVITY_REG_DEVICE_STATUS 0x008 /* R:  Device status bits */
#define GRAVITY_REG_FEATURES 0x00C      /* R:  Supported feature bits */

/* Shared memory info (read-only) */
#define GRAVITY_REG_SHMEM_SIZE_LO 0x010 /* R:  Shared memory size (low 32) */
#define GRAVITY_REG_SHMEM_SIZE_HI 0x014 /* R:  Shared memory size (high 32) */

/* Guest → Host doorbell (write-only) */
#define GRAVITY_REG_DOORBELL 0x020 /* W:  Writing any value notifies host of new commands in ring buffer */

/* Interrupt control */
#define GRAVITY_REG_IRQ_STATUS 0x030 /* R/W: Interrupt status (write 1 to clear) */
#define GRAVITY_REG_IRQ_MASK 0x034 /* R/W: Interrupt mask (1 = enabled) */

/* Guest feature negotiation */
#define GRAVITY_REG_GUEST_FEATURES 0x040 /* W:  Guest writes its feature bits here */
#define GRAVITY_REG_GUEST_STATUS 0x044 /* W:  Guest status (INIT, READY, ERROR) */

/* Display configuration */
#define GRAVITY_REG_DISPLAY_WIDTH 0x050  /* R/W: Display width in pixels */
#define GRAVITY_REG_DISPLAY_HEIGHT 0x054 /* R/W: Display height in pixels */
#define GRAVITY_REG_DISPLAY_FORMAT 0x058 /* R/W: Display pixel format */
#define GRAVITY_REG_DISPLAY_STRIDE 0x05C /* R:   Display stride (bytes per row) */

/* Debug */
#define GRAVITY_REG_DEBUG 0x0F0      /* R/W: Debug scratch register */
#define GRAVITY_REG_CMD_COUNT 0x0F4  /* R:   Total commands processed */
#define GRAVITY_REG_ERROR_CODE 0x0F8 /* R:   Last error code */

/* ═══════════════════════════════════════════════════════════════════════
 * Status and Feature Bits
 * ═══════════════════════════════════════════════════════════════════════ */

/* Device status bits (GRAVITY_REG_DEVICE_STATUS) */
#define GRAVITY_STATUS_READY (1 << 0)     /* Device initialized */
#define GRAVITY_STATUS_HOST_CONN (1 << 1) /* Host daemon connected */
#define GRAVITY_STATUS_ERROR (1 << 15)    /* Device error */

/* Feature bits (both device and guest) */
#define GRAVITY_FEAT_RING_BUFFER (1 << 0) /* Ring buffer transport */
#define GRAVITY_FEAT_SHADER_MSL (1 << 1)  /* MSL shader support */
#define GRAVITY_FEAT_SHADER_AIR (1 << 2)  /* Metal IR shader support */
#define GRAVITY_FEAT_COMPUTE (1 << 3)     /* Compute pipeline support */
#define GRAVITY_FEAT_BLIT (1 << 4)        /* Blit encoder support */
#define GRAVITY_FEAT_MSIX (1 << 5)        /* MSI-X interrupts */
#define GRAVITY_FEAT_DISPLAY (1 << 6)     /* Display/presentation support */

/* Guest status (GRAVITY_REG_GUEST_STATUS) */
#define GRAVITY_GUEST_INIT 0x01  /* Guest driver loaded */
#define GRAVITY_GUEST_READY 0x02 /* Guest completed negotiation */
#define GRAVITY_GUEST_ERROR 0xFF /* Guest encountered error */

/* IRQ bits */
#define GRAVITY_IRQ_CMD_COMPLETE (1 << 0) /* Command(s) completed */
#define GRAVITY_IRQ_HOST_EVENT (1 << 1)   /* Host sent event */
#define GRAVITY_IRQ_ERROR (1 << 7)        /* Error occurred */

/* ═══════════════════════════════════════════════════════════════════════
 * Limits
 * ═══════════════════════════════════════════════════════════════════════ */

#define GRAVITY_MAX_COLOR_ATTACHMENTS   8
#define GRAVITY_MAX_VERTEX_BUFFERS      31
#define GRAVITY_MAX_VERTEX_ATTRIBUTES   31
#define GRAVITY_MAX_BUFFER_ARGS         31
#define GRAVITY_MAX_TEXTURE_ARGS        128
#define GRAVITY_MAX_SAMPLER_ARGS        16
#define GRAVITY_MAX_VIEWPORTS           16
#define GRAVITY_MAX_SCISSOR_RECTS       16
#define GRAVITY_MAX_SHADER_SOURCE_SIZE  (1024 * 1024)  /* 1 MiB */
#define GRAVITY_MAX_LABEL_LENGTH        128

/* ═══════════════════════════════════════════════════════════════════════
 * Handle Types — Opaque 32-bit IDs for guest↔host object mapping
 * ═══════════════════════════════════════════════════════════════════════ */

typedef uint32_t gravity_handle_t;

#define GRAVITY_INVALID_HANDLE  ((gravity_handle_t)0)

/* Handle namespaces (upper 4 bits encode type for debugging) */
#define GRAVITY_HANDLE_TYPE_BUFFER          0x10000000
#define GRAVITY_HANDLE_TYPE_TEXTURE         0x20000000
#define GRAVITY_HANDLE_TYPE_SAMPLER         0x30000000
#define GRAVITY_HANDLE_TYPE_RENDER_PIPELINE 0x40000000
#define GRAVITY_HANDLE_TYPE_COMPUTE_PIPELINE 0x50000000
#define GRAVITY_HANDLE_TYPE_SHADER          0x60000000
#define GRAVITY_HANDLE_TYPE_DEPTH_STENCIL   0x70000000
#define GRAVITY_HANDLE_TYPE_FENCE           0x80000000
#define GRAVITY_HANDLE_TYPE_EVENT           0x90000000
#define GRAVITY_HANDLE_TYPE_CMDBUF          0xA0000000
#define GRAVITY_HANDLE_TYPE_MASK            0xF0000000
#define GRAVITY_HANDLE_INDEX_MASK           0x0FFFFFFF

/* ═══════════════════════════════════════════════════════════════════════
 * Command Opcodes
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum gravity_cmd_op {
    /* ── System ── */
    GRAVITY_CMD_NOP                     = 0x0000,
    GRAVITY_CMD_HELLO                   = 0x0001,  /* Feature negotiation */
    GRAVITY_CMD_GOODBYE                 = 0x0002,  /* Graceful shutdown */
    GRAVITY_CMD_PING                    = 0x0003,
    GRAVITY_CMD_PONG                    = 0x0004,

    /* ── Device Queries ── */
    GRAVITY_CMD_GET_DEVICE_INFO         = 0x0100,
    GRAVITY_CMD_DEVICE_INFO_RESPONSE    = 0x0101,

    /* ── Resource Creation ── */
    GRAVITY_CMD_CREATE_BUFFER           = 0x0200,
    GRAVITY_CMD_DESTROY_BUFFER          = 0x0201,
    GRAVITY_CMD_CREATE_TEXTURE          = 0x0210,
    GRAVITY_CMD_DESTROY_TEXTURE         = 0x0211,
    GRAVITY_CMD_CREATE_SAMPLER          = 0x0220,
    GRAVITY_CMD_DESTROY_SAMPLER         = 0x0221,

    /* ── Shader ── */
    GRAVITY_CMD_CREATE_SHADER           = 0x0300,
    GRAVITY_CMD_DESTROY_SHADER          = 0x0301,

    /* ── Pipeline State ── */
    GRAVITY_CMD_CREATE_RENDER_PIPELINE  = 0x0400,
    GRAVITY_CMD_DESTROY_RENDER_PIPELINE = 0x0401,
    GRAVITY_CMD_CREATE_COMPUTE_PIPELINE = 0x0410,
    GRAVITY_CMD_DESTROY_COMPUTE_PIPELINE= 0x0411,
    GRAVITY_CMD_CREATE_DEPTH_STENCIL    = 0x0420,
    GRAVITY_CMD_DESTROY_DEPTH_STENCIL   = 0x0421,

    /* ── Command Buffer Lifecycle ── */
    GRAVITY_CMD_CMDBUF_CREATE           = 0x0500,
    GRAVITY_CMD_CMDBUF_COMMIT           = 0x0501,
    GRAVITY_CMD_CMDBUF_WAIT             = 0x0502,
    GRAVITY_CMD_CMDBUF_COMPLETED        = 0x0503,  /* Host → Guest */

    /* ── Render Encoder ── */
    GRAVITY_CMD_RENDER_BEGIN             = 0x0600,
    GRAVITY_CMD_RENDER_END               = 0x0601,
    GRAVITY_CMD_RENDER_SET_PIPELINE      = 0x0610,
    GRAVITY_CMD_RENDER_SET_VERTEX_BUFFER = 0x0611,
    GRAVITY_CMD_RENDER_SET_FRAGMENT_BUFFER = 0x0612,
    GRAVITY_CMD_RENDER_SET_FRAGMENT_TEXTURE = 0x0613,
    GRAVITY_CMD_RENDER_SET_FRAGMENT_SAMPLER = 0x0614,
    GRAVITY_CMD_RENDER_SET_VERTEX_TEXTURE = 0x0615,
    GRAVITY_CMD_RENDER_SET_VERTEX_SAMPLER = 0x0616,
    GRAVITY_CMD_RENDER_SET_VIEWPORT      = 0x0620,
    GRAVITY_CMD_RENDER_SET_SCISSOR       = 0x0621,
    GRAVITY_CMD_RENDER_SET_CULL_MODE     = 0x0622,
    GRAVITY_CMD_RENDER_SET_WINDING       = 0x0623,
    GRAVITY_CMD_RENDER_SET_DEPTH_STENCIL = 0x0624,
    GRAVITY_CMD_RENDER_SET_DEPTH_BIAS    = 0x0625,
    GRAVITY_CMD_RENDER_SET_STENCIL_REF   = 0x0626,
    GRAVITY_CMD_RENDER_SET_BLEND_COLOR   = 0x0627,
    GRAVITY_CMD_RENDER_SET_TRIANGLE_FILL = 0x0628,
    GRAVITY_CMD_RENDER_DRAW              = 0x0640,
    GRAVITY_CMD_RENDER_DRAW_INDEXED      = 0x0641,
    GRAVITY_CMD_RENDER_DRAW_INSTANCED    = 0x0642,
    GRAVITY_CMD_RENDER_DRAW_INDEXED_INSTANCED = 0x0643,
    GRAVITY_CMD_RENDER_DRAW_INDIRECT     = 0x0644,

    /* ── Compute Encoder ── */
    GRAVITY_CMD_COMPUTE_BEGIN            = 0x0700,
    GRAVITY_CMD_COMPUTE_END              = 0x0701,
    GRAVITY_CMD_COMPUTE_SET_PIPELINE     = 0x0710,
    GRAVITY_CMD_COMPUTE_SET_BUFFER       = 0x0711,
    GRAVITY_CMD_COMPUTE_SET_TEXTURE      = 0x0712,
    GRAVITY_CMD_COMPUTE_SET_SAMPLER      = 0x0713,
    GRAVITY_CMD_COMPUTE_DISPATCH         = 0x0740,
    GRAVITY_CMD_COMPUTE_DISPATCH_INDIRECT = 0x0741,

    /* ── Blit Encoder ── */
    GRAVITY_CMD_BLIT_BEGIN               = 0x0800,
    GRAVITY_CMD_BLIT_END                 = 0x0801,
    GRAVITY_CMD_BLIT_COPY_BUFFER         = 0x0810,
    GRAVITY_CMD_BLIT_COPY_TEXTURE        = 0x0811,
    GRAVITY_CMD_BLIT_COPY_BUF_TO_TEX     = 0x0812,
    GRAVITY_CMD_BLIT_COPY_TEX_TO_BUF     = 0x0813,
    GRAVITY_CMD_BLIT_GENERATE_MIPMAPS    = 0x0820,
    GRAVITY_CMD_BLIT_FILL_BUFFER         = 0x0830,
    GRAVITY_CMD_BLIT_SYNCHRONIZE         = 0x0840,

    /* ── Synchronization ── */
    GRAVITY_CMD_CREATE_FENCE             = 0x0900,
    GRAVITY_CMD_DESTROY_FENCE            = 0x0901,
    GRAVITY_CMD_WAIT_FENCE               = 0x0902,
    GRAVITY_CMD_SIGNAL_FENCE             = 0x0903,
    GRAVITY_CMD_CREATE_EVENT             = 0x0910,
    GRAVITY_CMD_DESTROY_EVENT            = 0x0911,

    /* ── Data Transfer (bulk, via shared memory offset) ── */
    GRAVITY_CMD_UPLOAD_DATA              = 0x0A00,
    GRAVITY_CMD_READBACK_DATA            = 0x0A01,

    /* ── Display / Presentation ── */
    GRAVITY_CMD_PRESENT                  = 0x0B00,
    GRAVITY_CMD_RESIZE_DISPLAY           = 0x0B01,

    GRAVITY_CMD_MAX
} gravity_cmd_op_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Command Header — Every command starts with this
 * ═══════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)

typedef struct gravity_cmd_header {
    uint16_t    opcode;         /* gravity_cmd_op_t */
    uint16_t    flags;          /* Reserved */
    uint32_t    size;           /* Total size of this command including header */
    uint32_t    sequence;       /* Monotonically increasing sequence number */
    uint32_t    fence_id;       /* Optional fence to signal on completion */
} gravity_cmd_header_t;

_Static_assert(sizeof(gravity_cmd_header_t) == 16, "cmd_header must be 16 bytes");

/* ═══════════════════════════════════════════════════════════════════════
 * Feature Flags
 * ═══════════════════════════════════════════════════════════════════════ */

#define GRAVITY_FEAT_RING_BUFFER    (1 << 0)    /* Ring buffer transport */
#define GRAVITY_FEAT_SHADER_MSL     (1 << 1)    /* MSL shader support */
#define GRAVITY_FEAT_SHADER_AIR     (1 << 2)    /* Metal IR shader support */
#define GRAVITY_FEAT_COMPUTE        (1 << 3)    /* Compute pipeline support */
#define GRAVITY_FEAT_BLIT           (1 << 4)    /* Blit encoder support */
#define GRAVITY_FEAT_MSIX           (1 << 5)    /* MSI-X interrupts */
#define GRAVITY_FEAT_DISPLAY        (1 << 6)    /* Display/presentation support */

/* ═══════════════════════════════════════════════════════════════════════
 * System Commands
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_cmd_hello {
    gravity_cmd_header_t    hdr;
    uint32_t                magic;          /* GRAVITY_PROTOCOL_MAGIC */
    uint16_t                version_major;
    uint16_t                version_minor;
    uint32_t                guest_features; /* Feature bits */
    uint32_t                reserved[4];
} gravity_cmd_hello_t;

typedef struct gravity_cmd_hello_response {
    gravity_cmd_header_t    hdr;
    uint32_t                magic;
    uint16_t                version_major;
    uint16_t                version_minor;
    uint32_t                host_features;
    uint32_t                max_texture_size;
    uint32_t                max_buffer_size;
    uint32_t                max_threads_per_group;
    uint32_t                reserved[4];
} gravity_cmd_hello_response_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Metal Enum Mappings (subset — matches MTLPixelFormat values)
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum gravity_pixel_format {
    GRAVITY_PIXEL_FORMAT_INVALID            = 0,
    GRAVITY_PIXEL_FORMAT_A8_UNORM           = 1,
    GRAVITY_PIXEL_FORMAT_R8_UNORM           = 10,
    GRAVITY_PIXEL_FORMAT_R8_SNORM           = 12,
    GRAVITY_PIXEL_FORMAT_R8_UINT            = 13,
    GRAVITY_PIXEL_FORMAT_R16_UINT           = 23,
    GRAVITY_PIXEL_FORMAT_R16_FLOAT          = 25,
    GRAVITY_PIXEL_FORMAT_RG8_UNORM          = 30,
    GRAVITY_PIXEL_FORMAT_RG8_SNORM          = 32,
    GRAVITY_PIXEL_FORMAT_R32_UINT           = 53,
    GRAVITY_PIXEL_FORMAT_R32_FLOAT          = 55,
    GRAVITY_PIXEL_FORMAT_RG16_UINT          = 63,
    GRAVITY_PIXEL_FORMAT_RG16_FLOAT         = 65,
    GRAVITY_PIXEL_FORMAT_RGBA8_UNORM        = 70,
    GRAVITY_PIXEL_FORMAT_RGBA8_UNORM_SRGB   = 71,
    GRAVITY_PIXEL_FORMAT_RGBA8_SNORM        = 72,
    GRAVITY_PIXEL_FORMAT_RGBA8_UINT         = 73,
    GRAVITY_PIXEL_FORMAT_BGRA8_UNORM        = 80,
    GRAVITY_PIXEL_FORMAT_BGRA8_UNORM_SRGB   = 81,
    GRAVITY_PIXEL_FORMAT_RGB10A2_UNORM      = 90,
    GRAVITY_PIXEL_FORMAT_RG32_UINT          = 105,
    GRAVITY_PIXEL_FORMAT_RG32_FLOAT         = 107,
    GRAVITY_PIXEL_FORMAT_RGBA16_UINT        = 113,
    GRAVITY_PIXEL_FORMAT_RGBA16_FLOAT       = 115,
    GRAVITY_PIXEL_FORMAT_RGBA32_UINT        = 123,
    GRAVITY_PIXEL_FORMAT_RGBA32_FLOAT       = 125,
    GRAVITY_PIXEL_FORMAT_DEPTH16_UNORM      = 250,
    GRAVITY_PIXEL_FORMAT_DEPTH32_FLOAT      = 252,
    GRAVITY_PIXEL_FORMAT_STENCIL8           = 253,
    GRAVITY_PIXEL_FORMAT_DEPTH24_STENCIL8   = 255,
    GRAVITY_PIXEL_FORMAT_DEPTH32_FLOAT_STENCIL8 = 260,
} gravity_pixel_format_t;

typedef enum gravity_texture_type {
    GRAVITY_TEXTURE_TYPE_1D         = 0,
    GRAVITY_TEXTURE_TYPE_1D_ARRAY   = 1,
    GRAVITY_TEXTURE_TYPE_2D         = 2,
    GRAVITY_TEXTURE_TYPE_2D_ARRAY   = 3,
    GRAVITY_TEXTURE_TYPE_2D_MS      = 4,
    GRAVITY_TEXTURE_TYPE_CUBE       = 5,
    GRAVITY_TEXTURE_TYPE_CUBE_ARRAY = 6,
    GRAVITY_TEXTURE_TYPE_3D         = 7,
} gravity_texture_type_t;

typedef enum gravity_storage_mode {
    GRAVITY_STORAGE_SHARED      = 0,
    GRAVITY_STORAGE_MANAGED     = 1,
    GRAVITY_STORAGE_PRIVATE     = 2,
    GRAVITY_STORAGE_MEMORYLESS  = 3,
} gravity_storage_mode_t;

typedef enum gravity_load_action {
    GRAVITY_LOAD_DONT_CARE  = 0,
    GRAVITY_LOAD_LOAD       = 1,
    GRAVITY_LOAD_CLEAR      = 2,
} gravity_load_action_t;

typedef enum gravity_store_action {
    GRAVITY_STORE_DONT_CARE         = 0,
    GRAVITY_STORE_STORE             = 1,
    GRAVITY_STORE_MULTISAMPLE_RESOLVE = 2,
    GRAVITY_STORE_STORE_AND_MULTISAMPLE_RESOLVE = 3,
    GRAVITY_STORE_UNKNOWN           = 4,
    GRAVITY_STORE_CUSTOM_SAMPLE_DEPTH_STORE = 5,
} gravity_store_action_t;

typedef enum gravity_primitive_type {
    GRAVITY_PRIMITIVE_POINT          = 0,
    GRAVITY_PRIMITIVE_LINE           = 1,
    GRAVITY_PRIMITIVE_LINE_STRIP     = 2,
    GRAVITY_PRIMITIVE_TRIANGLE       = 3,
    GRAVITY_PRIMITIVE_TRIANGLE_STRIP = 4,
} gravity_primitive_type_t;

typedef enum gravity_index_type {
    GRAVITY_INDEX_UINT16    = 0,
    GRAVITY_INDEX_UINT32    = 1,
} gravity_index_type_t;

typedef enum gravity_cull_mode {
    GRAVITY_CULL_NONE   = 0,
    GRAVITY_CULL_FRONT  = 1,
    GRAVITY_CULL_BACK   = 2,
} gravity_cull_mode_t;

typedef enum gravity_winding {
    GRAVITY_WINDING_CW  = 0,
    GRAVITY_WINDING_CCW = 1,
} gravity_winding_t;

typedef enum gravity_triangle_fill_mode {
    GRAVITY_FILL_FILL       = 0,
    GRAVITY_FILL_LINES      = 1,
} gravity_triangle_fill_mode_t;

typedef enum gravity_compare_function {
    GRAVITY_COMPARE_NEVER           = 0,
    GRAVITY_COMPARE_LESS            = 1,
    GRAVITY_COMPARE_EQUAL           = 2,
    GRAVITY_COMPARE_LESS_EQUAL      = 3,
    GRAVITY_COMPARE_GREATER         = 4,
    GRAVITY_COMPARE_NOT_EQUAL       = 5,
    GRAVITY_COMPARE_GREATER_EQUAL   = 6,
    GRAVITY_COMPARE_ALWAYS          = 7,
} gravity_compare_function_t;

typedef enum gravity_sampler_address_mode {
    GRAVITY_ADDRESS_CLAMP_TO_EDGE    = 0,
    GRAVITY_ADDRESS_MIRROR_CLAMP     = 1,
    GRAVITY_ADDRESS_REPEAT           = 2,
    GRAVITY_ADDRESS_MIRROR_REPEAT    = 3,
    GRAVITY_ADDRESS_CLAMP_TO_ZERO    = 4,
    GRAVITY_ADDRESS_CLAMP_TO_BORDER  = 5,
} gravity_sampler_address_mode_t;

typedef enum gravity_sampler_min_mag_filter {
    GRAVITY_FILTER_NEAREST  = 0,
    GRAVITY_FILTER_LINEAR   = 1,
} gravity_sampler_min_mag_filter_t;

typedef enum gravity_sampler_mip_filter {
    GRAVITY_MIP_NOT_MIPMAPPED   = 0,
    GRAVITY_MIP_NEAREST         = 1,
    GRAVITY_MIP_LINEAR          = 2,
} gravity_sampler_mip_filter_t;

typedef enum gravity_blend_factor {
    GRAVITY_BLEND_ZERO                      = 0,
    GRAVITY_BLEND_ONE                       = 1,
    GRAVITY_BLEND_SRC_COLOR                 = 2,
    GRAVITY_BLEND_ONE_MINUS_SRC_COLOR       = 3,
    GRAVITY_BLEND_SRC_ALPHA                 = 4,
    GRAVITY_BLEND_ONE_MINUS_SRC_ALPHA       = 5,
    GRAVITY_BLEND_DST_COLOR                 = 6,
    GRAVITY_BLEND_ONE_MINUS_DST_COLOR       = 7,
    GRAVITY_BLEND_DST_ALPHA                 = 8,
    GRAVITY_BLEND_ONE_MINUS_DST_ALPHA       = 9,
    GRAVITY_BLEND_SRC_ALPHA_SATURATED       = 10,
    GRAVITY_BLEND_BLEND_COLOR               = 11,
    GRAVITY_BLEND_ONE_MINUS_BLEND_COLOR     = 12,
    GRAVITY_BLEND_BLEND_ALPHA               = 13,
    GRAVITY_BLEND_ONE_MINUS_BLEND_ALPHA     = 14,
} gravity_blend_factor_t;

typedef enum gravity_blend_operation {
    GRAVITY_BLEND_OP_ADD                = 0,
    GRAVITY_BLEND_OP_SUBTRACT           = 1,
    GRAVITY_BLEND_OP_REVERSE_SUBTRACT   = 2,
    GRAVITY_BLEND_OP_MIN                = 3,
    GRAVITY_BLEND_OP_MAX                = 4,
} gravity_blend_operation_t;

typedef enum gravity_vertex_format {
    GRAVITY_VERTEX_FORMAT_INVALID   = 0,
    GRAVITY_VERTEX_FORMAT_FLOAT     = 28,
    GRAVITY_VERTEX_FORMAT_FLOAT2    = 29,
    GRAVITY_VERTEX_FORMAT_FLOAT3    = 30,
    GRAVITY_VERTEX_FORMAT_FLOAT4    = 31,
    GRAVITY_VERTEX_FORMAT_UCHAR4    = 45,
    GRAVITY_VERTEX_FORMAT_UCHAR4_NORM = 46,
    GRAVITY_VERTEX_FORMAT_SHORT2    = 52,
    GRAVITY_VERTEX_FORMAT_SHORT4    = 54,
    GRAVITY_VERTEX_FORMAT_HALF2     = 56,
    GRAVITY_VERTEX_FORMAT_HALF4     = 58,
    GRAVITY_VERTEX_FORMAT_INT       = 33,
    GRAVITY_VERTEX_FORMAT_INT2      = 34,
    GRAVITY_VERTEX_FORMAT_INT3      = 35,
    GRAVITY_VERTEX_FORMAT_INT4      = 36,
    GRAVITY_VERTEX_FORMAT_UINT      = 37,
} gravity_vertex_format_t;

typedef enum gravity_vertex_step_function {
    GRAVITY_STEP_CONSTANT           = 0,
    GRAVITY_STEP_PER_VERTEX         = 1,
    GRAVITY_STEP_PER_INSTANCE       = 2,
    GRAVITY_STEP_PER_PATCH          = 3,
    GRAVITY_STEP_PER_PATCH_CONTROL  = 4,
} gravity_vertex_step_function_t;

typedef enum gravity_stencil_operation {
    GRAVITY_STENCIL_OP_KEEP             = 0,
    GRAVITY_STENCIL_OP_ZERO             = 1,
    GRAVITY_STENCIL_OP_REPLACE          = 2,
    GRAVITY_STENCIL_OP_INCREMENT_CLAMP  = 3,
    GRAVITY_STENCIL_OP_DECREMENT_CLAMP  = 4,
    GRAVITY_STENCIL_OP_INVERT           = 5,
    GRAVITY_STENCIL_OP_INCREMENT_WRAP   = 6,
    GRAVITY_STENCIL_OP_DECREMENT_WRAP   = 7,
} gravity_stencil_operation_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Resource Creation Commands
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_cmd_create_buffer {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;         /* Assigned by guest */
    uint64_t                size;           /* Buffer size in bytes */
    uint32_t                storage_mode;   /* gravity_storage_mode_t */
    uint32_t                usage;          /* Bitmask of usage flags */
    uint64_t                data_offset;    /* Offset in shared memory for initial data (0 = none) */
    uint32_t                data_size;      /* Size of initial data */
    uint32_t                reserved;
} gravity_cmd_create_buffer_t;

typedef struct gravity_cmd_destroy_buffer {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
} gravity_cmd_destroy_buffer_t;

typedef struct gravity_cmd_create_texture {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
    uint32_t                texture_type;       /* gravity_texture_type_t */
    uint32_t                pixel_format;       /* gravity_pixel_format_t */
    uint32_t                width;
    uint32_t                height;
    uint32_t                depth;
    uint32_t                mipmap_levels;
    uint32_t                array_length;
    uint32_t                sample_count;
    uint32_t                storage_mode;       /* gravity_storage_mode_t */
    uint32_t                usage;              /* Bitmask: shader_read, shader_write, render_target, etc. */
    uint64_t                data_offset;        /* Offset in shared memory for initial data */
    uint32_t                data_size;
    uint32_t                reserved;
} gravity_cmd_create_texture_t;

typedef struct gravity_cmd_destroy_texture {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
} gravity_cmd_destroy_texture_t;

typedef struct gravity_cmd_create_sampler {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
    uint32_t                min_filter;         /* gravity_sampler_min_mag_filter_t */
    uint32_t                mag_filter;
    uint32_t                mip_filter;         /* gravity_sampler_mip_filter_t */
    uint32_t                address_mode_s;     /* gravity_sampler_address_mode_t */
    uint32_t                address_mode_t;
    uint32_t                address_mode_r;
    float                   lod_min_clamp;
    float                   lod_max_clamp;
    uint32_t                max_anisotropy;
    uint32_t                compare_function;   /* gravity_compare_function_t */
    uint32_t                reserved;
} gravity_cmd_create_sampler_t;

typedef struct gravity_cmd_destroy_sampler {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
} gravity_cmd_destroy_sampler_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Shader Commands
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum gravity_shader_type {
    GRAVITY_SHADER_VERTEX       = 0,
    GRAVITY_SHADER_FRAGMENT     = 1,
    GRAVITY_SHADER_COMPUTE      = 2,
} gravity_shader_type_t;

typedef enum gravity_shader_source_type {
    GRAVITY_SHADER_SOURCE_MSL       = 0,    /* Metal Shading Language text */
    GRAVITY_SHADER_SOURCE_METALLIB  = 1,    /* Precompiled .metallib */
    GRAVITY_SHADER_SOURCE_AIR       = 2,    /* Metal IR (Apple IR / LLVM bitcode) */
} gravity_shader_source_type_t;

typedef struct gravity_cmd_create_shader {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
    uint32_t                shader_type;        /* gravity_shader_type_t */
    uint32_t                source_type;        /* gravity_shader_source_type_t */
    uint64_t                source_offset;      /* Offset in shared memory */
    uint32_t                source_size;        /* Size of shader source/bytecode */
    char                    entry_point[64];    /* Function name (e.g., "vertexShader") */
    uint32_t                reserved;
} gravity_cmd_create_shader_t;

typedef struct gravity_cmd_destroy_shader {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
} gravity_cmd_destroy_shader_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Pipeline State
 * ═══════════════════════════════════════════════════════════════════════ */

/* Vertex attribute descriptor */
typedef struct gravity_vertex_attribute_desc {
    uint32_t    format;         /* gravity_vertex_format_t */
    uint32_t    offset;
    uint32_t    buffer_index;
    uint32_t    reserved;
} gravity_vertex_attribute_desc_t;

/* Vertex buffer layout descriptor */
typedef struct gravity_vertex_buffer_layout_desc {
    uint32_t    stride;
    uint32_t    step_function;  /* gravity_vertex_step_function_t */
    uint32_t    step_rate;
    uint32_t    reserved;
} gravity_vertex_buffer_layout_desc_t;

/* Color attachment blend state */
typedef struct gravity_color_attachment_desc {
    uint32_t    pixel_format;           /* gravity_pixel_format_t */
    uint8_t     blending_enabled;
    uint8_t     write_mask;             /* RGBA bitmask */
    uint8_t     reserved[2];
    uint32_t    rgb_blend_operation;    /* gravity_blend_operation_t */
    uint32_t    alpha_blend_operation;
    uint32_t    src_rgb_blend_factor;   /* gravity_blend_factor_t */
    uint32_t    dst_rgb_blend_factor;
    uint32_t    src_alpha_blend_factor;
    uint32_t    dst_alpha_blend_factor;
} gravity_color_attachment_desc_t;

/* Stencil descriptor */
typedef struct gravity_stencil_desc {
    uint32_t    stencil_compare;    /* gravity_compare_function_t */
    uint32_t    stencil_fail_op;    /* gravity_stencil_operation_t */
    uint32_t    depth_fail_op;
    uint32_t    depth_stencil_pass_op;
    uint32_t    read_mask;
    uint32_t    write_mask;
} gravity_stencil_desc_t;

typedef struct gravity_cmd_create_render_pipeline {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
    gravity_handle_t        vertex_shader;
    gravity_handle_t        fragment_shader;    /* GRAVITY_INVALID_HANDLE if none */

    /* Vertex descriptor */
    uint32_t                    num_vertex_attributes;
    gravity_vertex_attribute_desc_t     vertex_attributes[GRAVITY_MAX_VERTEX_ATTRIBUTES];
    uint32_t                    num_vertex_buffer_layouts;
    gravity_vertex_buffer_layout_desc_t vertex_buffer_layouts[GRAVITY_MAX_VERTEX_BUFFERS];

    /* Color attachments */
    uint32_t                    num_color_attachments;
    gravity_color_attachment_desc_t     color_attachments[GRAVITY_MAX_COLOR_ATTACHMENTS];

    /* Depth/stencil */
    uint32_t                depth_attachment_format;     /* gravity_pixel_format_t */
    uint32_t                stencil_attachment_format;

    /* Rasterization */
    uint32_t                sample_count;
    uint8_t                 alpha_to_coverage_enabled;
    uint8_t                 alpha_to_one_enabled;
    uint8_t                 rasterization_enabled;
    uint8_t                 reserved_pad;
    uint32_t                reserved;
} gravity_cmd_create_render_pipeline_t;

typedef struct gravity_cmd_destroy_render_pipeline {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
} gravity_cmd_destroy_render_pipeline_t;

typedef struct gravity_cmd_create_compute_pipeline {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
    gravity_handle_t        compute_shader;
    uint32_t                thread_group_size_x;
    uint32_t                thread_group_size_y;
    uint32_t                thread_group_size_z;
    uint32_t                reserved;
} gravity_cmd_create_compute_pipeline_t;

typedef struct gravity_cmd_create_depth_stencil {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
    uint32_t                depth_compare;      /* gravity_compare_function_t */
    uint8_t                 depth_write_enabled;
    uint8_t                 reserved_pad[3];
    gravity_stencil_desc_t  front_face_stencil;
    gravity_stencil_desc_t  back_face_stencil;
} gravity_cmd_create_depth_stencil_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Render Encoder Commands
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_color_attachment_action {
    gravity_handle_t    texture;
    uint32_t            level;
    uint32_t            slice;
    uint32_t            load_action;    /* gravity_load_action_t */
    uint32_t            store_action;   /* gravity_store_action_t */
    float               clear_color[4]; /* RGBA */
} gravity_color_attachment_action_t;

typedef struct gravity_depth_attachment_action {
    gravity_handle_t    texture;
    uint32_t            level;
    uint32_t            slice;
    uint32_t            load_action;
    uint32_t            store_action;
    float               clear_depth;
    uint32_t            reserved;
} gravity_depth_attachment_action_t;

typedef struct gravity_stencil_attachment_action {
    gravity_handle_t    texture;
    uint32_t            level;
    uint32_t            slice;
    uint32_t            load_action;
    uint32_t            store_action;
    uint32_t            clear_stencil;
    uint32_t            reserved;
} gravity_stencil_attachment_action_t;

typedef struct gravity_cmd_render_begin {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    uint32_t                num_color_attachments;
    gravity_color_attachment_action_t   color_attachments[GRAVITY_MAX_COLOR_ATTACHMENTS];
    gravity_depth_attachment_action_t   depth_attachment;
    gravity_stencil_attachment_action_t stencil_attachment;
    uint32_t                render_area_x;
    uint32_t                render_area_y;
    uint32_t                render_area_width;
    uint32_t                render_area_height;
} gravity_cmd_render_begin_t;

typedef struct gravity_cmd_render_end {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
} gravity_cmd_render_end_t;

typedef struct gravity_viewport {
    float   x, y;
    float   width, height;
    float   znear, zfar;
} gravity_viewport_t;

typedef struct gravity_scissor_rect {
    uint32_t x, y;
    uint32_t width, height;
} gravity_scissor_rect_t;

typedef struct gravity_cmd_render_set_pipeline {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        pipeline;
} gravity_cmd_render_set_pipeline_t;

typedef struct gravity_cmd_render_set_vertex_buffer {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        buffer;
    uint32_t                offset;
    uint32_t                index;
} gravity_cmd_render_set_vertex_buffer_t;

typedef struct gravity_cmd_render_set_fragment_buffer {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        buffer;
    uint32_t                offset;
    uint32_t                index;
} gravity_cmd_render_set_fragment_buffer_t;

typedef struct gravity_cmd_render_set_fragment_texture {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        texture;
    uint32_t                index;
} gravity_cmd_render_set_fragment_texture_t;

typedef struct gravity_cmd_render_set_fragment_sampler {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        sampler;
    uint32_t                index;
} gravity_cmd_render_set_fragment_sampler_t;

typedef struct gravity_cmd_render_set_viewport {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_viewport_t      viewport;
} gravity_cmd_render_set_viewport_t;

typedef struct gravity_cmd_render_set_scissor {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_scissor_rect_t  rect;
} gravity_cmd_render_set_scissor_t;

typedef struct gravity_cmd_render_set_cull_mode {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    uint32_t                cull_mode;  /* gravity_cull_mode_t */
} gravity_cmd_render_set_cull_mode_t;

typedef struct gravity_cmd_render_set_depth_stencil {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        depth_stencil_state;
} gravity_cmd_render_set_depth_stencil_t;

typedef struct gravity_cmd_render_set_depth_bias {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    float                   depth_bias;
    float                   slope_scale;
    float                   clamp;
} gravity_cmd_render_set_depth_bias_t;

typedef struct gravity_cmd_render_set_blend_color {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    float                   color[4];   /* RGBA */
} gravity_cmd_render_set_blend_color_t;

typedef struct gravity_cmd_render_draw {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    uint32_t                primitive_type;     /* gravity_primitive_type_t */
    uint32_t                vertex_start;
    uint32_t                vertex_count;
    uint32_t                reserved;
} gravity_cmd_render_draw_t;

typedef struct gravity_cmd_render_draw_indexed {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    uint32_t                primitive_type;
    uint32_t                index_count;
    uint32_t                index_type;         /* gravity_index_type_t */
    gravity_handle_t        index_buffer;
    uint32_t                index_buffer_offset;
    uint32_t                reserved;
} gravity_cmd_render_draw_indexed_t;

typedef struct gravity_cmd_render_draw_instanced {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    uint32_t                primitive_type;
    uint32_t                vertex_start;
    uint32_t                vertex_count;
    uint32_t                instance_count;
    uint32_t                base_instance;
    uint32_t                reserved;
} gravity_cmd_render_draw_instanced_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Compute Encoder Commands
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_cmd_compute_begin {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
} gravity_cmd_compute_begin_t;

typedef struct gravity_cmd_compute_end {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
} gravity_cmd_compute_end_t;

typedef struct gravity_cmd_compute_set_pipeline {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        pipeline;
} gravity_cmd_compute_set_pipeline_t;

typedef struct gravity_cmd_compute_set_buffer {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        buffer;
    uint32_t                offset;
    uint32_t                index;
} gravity_cmd_compute_set_buffer_t;

typedef struct gravity_cmd_compute_dispatch {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    uint32_t                threadgroups_x;
    uint32_t                threadgroups_y;
    uint32_t                threadgroups_z;
    uint32_t                threads_per_group_x;
    uint32_t                threads_per_group_y;
    uint32_t                threads_per_group_z;
} gravity_cmd_compute_dispatch_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Blit Encoder Commands
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_origin {
    uint32_t x, y, z;
} gravity_origin_t;

typedef struct gravity_size {
    uint32_t width, height, depth;
} gravity_size_t;

typedef struct gravity_cmd_blit_copy_buffer {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        src_buffer;
    uint64_t                src_offset;
    gravity_handle_t        dst_buffer;
    uint64_t                dst_offset;
    uint64_t                size;
} gravity_cmd_blit_copy_buffer_t;

typedef struct gravity_cmd_blit_copy_texture {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        src_texture;
    uint32_t                src_slice;
    uint32_t                src_level;
    gravity_origin_t        src_origin;
    gravity_size_t          src_size;
    gravity_handle_t        dst_texture;
    uint32_t                dst_slice;
    uint32_t                dst_level;
    gravity_origin_t        dst_origin;
} gravity_cmd_blit_copy_texture_t;

typedef struct gravity_cmd_blit_copy_buf_to_tex {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        src_buffer;
    uint64_t                src_offset;
    uint32_t                src_bytes_per_row;
    uint32_t                src_bytes_per_image;
    gravity_size_t          src_size;
    gravity_handle_t        dst_texture;
    uint32_t                dst_slice;
    uint32_t                dst_level;
    gravity_origin_t        dst_origin;
} gravity_cmd_blit_copy_buf_to_tex_t;

typedef struct gravity_cmd_blit_generate_mipmaps {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        cmdbuf;
    gravity_handle_t        texture;
} gravity_cmd_blit_generate_mipmaps_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Synchronization Commands
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_cmd_create_fence {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
} gravity_cmd_create_fence_t;

typedef struct gravity_cmd_wait_fence {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        fence;
    uint64_t                timeout_ns;     /* 0 = non-blocking, UINT64_MAX = infinite */
} gravity_cmd_wait_fence_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Data Transfer Commands
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_cmd_upload_data {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        target;         /* Buffer or texture handle */
    uint64_t                target_offset;  /* Offset within the target resource */
    uint64_t                data_offset;    /* Offset in shared memory where data resides */
    uint32_t                data_size;      /* Size of data to upload */
    uint32_t                reserved;
} gravity_cmd_upload_data_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Display / Presentation Commands
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_cmd_present {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        texture;        /* Texture to present */
    uint32_t                reserved;
} gravity_cmd_present_t;

typedef struct gravity_cmd_resize_display {
    gravity_cmd_header_t    hdr;
    uint32_t                width;
    uint32_t                height;
    uint32_t                pixel_format;   /* gravity_pixel_format_t */
    uint32_t                reserved;
} gravity_cmd_resize_display_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Command Buffer Lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_cmd_cmdbuf_create {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
} gravity_cmd_cmdbuf_create_t;

typedef struct gravity_cmd_cmdbuf_commit {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
} gravity_cmd_cmdbuf_commit_t;

typedef struct gravity_cmd_cmdbuf_completed {
    gravity_cmd_header_t    hdr;
    gravity_handle_t        handle;
    uint32_t                status;     /* 0 = success, nonzero = error code */
    uint64_t                gpu_time_ns;
} gravity_cmd_cmdbuf_completed_t;

#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════
 * Protocol Helper Functions
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Get human-readable name for a command opcode.
 */
const char* gravity_cmd_name(uint16_t opcode);

/**
 * Validate a command header.
 * Returns 0 on success, -1 on invalid.
 */
int gravity_cmd_validate(const gravity_cmd_header_t* hdr, size_t available_bytes);

/**
 * Initialize a command header.
 */
static inline void gravity_cmd_init(gravity_cmd_header_t* hdr,
                                     uint16_t opcode,
                                     uint32_t size,
                                     uint32_t sequence)
{
    hdr->opcode   = opcode;
    hdr->flags    = 0;
    hdr->size     = size;
    hdr->sequence = sequence;
    hdr->fence_id = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* GRAVITY_PROTOCOL_H */
