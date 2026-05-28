/*
 * cmd_processor.h — Command Processor Declarations
 *
 * Declares the command handler functions that process each type of
 * serialized Metal command and translate it to Vulkan operations.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#ifndef GRAVITY_CMD_PROCESSOR_H
#define GRAVITY_CMD_PROCESSOR_H

#include "../../protocol/include/gravity_protocol.h"
#include "../../protocol/include/gravity_ring.h"
#include "resource_tracker.h"
#include "vk_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════
 * Resource Creation Handlers
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_create_buffer(gravity_vk_state_t *vk,
                                       gravity_resource_tracker_t *tracker,
                                       gravity_ring_t *ring,
                                       const gravity_cmd_create_buffer_t *cmd);

void gravity_cmd_process_create_texture(
    gravity_vk_state_t *vk, gravity_resource_tracker_t *tracker,
    gravity_ring_t *ring, const gravity_cmd_create_texture_t *cmd);

void gravity_cmd_process_create_sampler(
    gravity_vk_state_t *vk, gravity_resource_tracker_t *tracker,
    const gravity_cmd_create_sampler_t *cmd);

void gravity_cmd_process_create_shader(gravity_vk_state_t *vk,
                                       gravity_resource_tracker_t *tracker,
                                       gravity_ring_t *ring,
                                       const gravity_cmd_create_shader_t *cmd);

void gravity_cmd_process_create_render_pipeline(
    gravity_vk_state_t *vk, gravity_resource_tracker_t *tracker,
    const gravity_cmd_create_render_pipeline_t *cmd);

void gravity_cmd_process_create_depth_stencil(
    gravity_vk_state_t *vk, gravity_resource_tracker_t *tracker,
    const gravity_cmd_create_depth_stencil_t *cmd);

/* ═══════════════════════════════════════════════════════════════════════
 * Command Buffer Handlers
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_cmdbuf_create(gravity_vk_state_t *vk,
                                       gravity_resource_tracker_t *tracker,
                                       const gravity_cmd_cmdbuf_create_t *cmd);

void gravity_cmd_process_cmdbuf_commit(gravity_vk_state_t *vk,
                                       gravity_resource_tracker_t *tracker,
                                       gravity_ring_t *ring,
                                       const gravity_cmd_cmdbuf_commit_t *cmd);

/* ═══════════════════════════════════════════════════════════════════════
 * Encoder Handlers (dispatched by opcode category)
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_render(gravity_vk_state_t *vk,
                                gravity_resource_tracker_t *tracker,
                                gravity_ring_t *ring,
                                const gravity_cmd_header_t *hdr,
                                const void *cmd_data);

void gravity_cmd_process_compute(gravity_vk_state_t *vk,
                                 gravity_resource_tracker_t *tracker,
                                 gravity_ring_t *ring,
                                 const gravity_cmd_header_t *hdr,
                                 const void *cmd_data);

void gravity_cmd_process_blit(gravity_vk_state_t *vk,
                              gravity_resource_tracker_t *tracker,
                              gravity_ring_t *ring,
                              const gravity_cmd_header_t *hdr,
                              const void *cmd_data);

/* ═══════════════════════════════════════════════════════════════════════
 * Presentation Handlers
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_present(gravity_vk_state_t *vk,
                                 gravity_resource_tracker_t *tracker,
                                 gravity_ring_t *ring,
                                 const gravity_cmd_present_t *cmd);

void gravity_cmd_process_resize_display(
    gravity_vk_state_t *vk, const gravity_cmd_resize_display_t *cmd);

#ifdef __cplusplus
}
#endif

#endif /* GRAVITY_CMD_PROCESSOR_H */
