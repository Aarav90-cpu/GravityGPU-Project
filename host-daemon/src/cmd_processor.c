/*
 * cmd_processor.c — Command Processor Implementation
 *
 * Processes each Metal command by creating/managing Vulkan resources
 * and recording Vulkan commands. This is where Metal→Vulkan
 * translation actually happens.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include "cmd_processor.h"
#include "metal_to_vulkan.h"
#include "gravity_formats.h"
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Buffer Creation
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_create_buffer(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    gravity_ring_t* ring,
    const gravity_cmd_create_buffer_t* cmd)
{
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = cmd->size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkBuffer buffer;
    if (vkCreateBuffer(vk->device, &buf_info, NULL, &buffer) != VK_SUCCESS) {
        fprintf(stderr, "[cmd_proc] Failed to create buffer handle=0x%08x\n",
                cmd->handle);
        return;
    }

    /* Allocate memory */
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(vk->device, buffer, &mem_reqs);

    VkMemoryPropertyFlags mem_props =
        gravity_translate_storage_mode(cmd->storage_mode);

    uint32_t mem_type = gravity_vk_find_memory_type(vk, mem_reqs.memoryTypeBits,
                                                     mem_props);
    if (mem_type == UINT32_MAX) {
        /* Fall back to any host-visible memory */
        mem_type = gravity_vk_find_memory_type(vk, mem_reqs.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type,
    };

    VkDeviceMemory memory;
    if (vkAllocateMemory(vk->device, &alloc_info, NULL, &memory) != VK_SUCCESS) {
        fprintf(stderr, "[cmd_proc] Failed to allocate buffer memory\n");
        vkDestroyBuffer(vk->device, buffer, NULL);
        return;
    }

    vkBindBufferMemory(vk->device, buffer, memory, 0);

    /* Track the resource */
    gravity_resource_entry_t* entry =
        gravity_resource_tracker_add(tracker, cmd->handle, GRAVITY_RES_BUFFER);
    if (!entry) {
        vkFreeMemory(vk->device, memory, NULL);
        vkDestroyBuffer(vk->device, buffer, NULL);
        return;
    }

    entry->buffer.buffer = buffer;
    entry->buffer.memory = memory;
    entry->buffer.size = cmd->size;
    entry->buffer.mapped = NULL;

    /* If shared storage, persistently map */
    if (cmd->storage_mode == GRAVITY_STORAGE_SHARED ||
        cmd->storage_mode == GRAVITY_STORAGE_MANAGED) {
        vkMapMemory(vk->device, memory, 0, cmd->size, 0, &entry->buffer.mapped);
    }

    /* If initial data provided, copy it */
    if (cmd->data_offset > 0 && cmd->data_size > 0 && entry->buffer.mapped) {
        void* src = gravity_ring_data_ptr(ring, cmd->data_offset);
        if (src) {
            memcpy(entry->buffer.mapped, src, cmd->data_size);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Texture Creation
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_create_texture(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    gravity_ring_t* ring,
    const gravity_cmd_create_texture_t* cmd)
{
    VkFormat vk_format = (VkFormat)gravity_metal_to_vk_pixel_format(cmd->pixel_format);
    if (vk_format == VK_FORMAT_UNDEFINED) {
        fprintf(stderr, "[cmd_proc] Unsupported pixel format %u for texture 0x%08x\n",
                cmd->pixel_format, cmd->handle);
        return;
    }

    VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = gravity_translate_texture_type(cmd->texture_type),
        .format = vk_format,
        .extent = {
            .width = cmd->width > 0 ? cmd->width : 1,
            .height = cmd->height > 0 ? cmd->height : 1,
            .depth = cmd->depth > 0 ? cmd->depth : 1,
        },
        .mipLevels = cmd->mipmap_levels > 0 ? cmd->mipmap_levels : 1,
        .arrayLayers = cmd->array_length > 0 ? cmd->array_length : 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = gravity_translate_texture_usage(cmd->usage, cmd->pixel_format),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    /* Handle cube maps */
    if (cmd->texture_type == GRAVITY_TEXTURE_TYPE_CUBE ||
        cmd->texture_type == GRAVITY_TEXTURE_TYPE_CUBE_ARRAY) {
        img_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        img_info.arrayLayers *= 6;
    }

    /* Handle multisampling */
    if (cmd->sample_count > 1) {
        if (cmd->sample_count <= 2)       img_info.samples = VK_SAMPLE_COUNT_2_BIT;
        else if (cmd->sample_count <= 4)  img_info.samples = VK_SAMPLE_COUNT_4_BIT;
        else if (cmd->sample_count <= 8)  img_info.samples = VK_SAMPLE_COUNT_8_BIT;
        else                              img_info.samples = VK_SAMPLE_COUNT_16_BIT;
    }

    VkImage image;
    if (vkCreateImage(vk->device, &img_info, NULL, &image) != VK_SUCCESS) {
        fprintf(stderr, "[cmd_proc] Failed to create image handle=0x%08x\n",
                cmd->handle);
        return;
    }

    /* Allocate memory */
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(vk->device, image, &mem_reqs);

    VkMemoryPropertyFlags mem_props =
        gravity_translate_storage_mode(cmd->storage_mode);

    uint32_t mem_type = gravity_vk_find_memory_type(vk, mem_reqs.memoryTypeBits,
                                                     mem_props);
    if (mem_type == UINT32_MAX) {
        mem_type = gravity_vk_find_memory_type(vk, mem_reqs.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type,
    };

    VkDeviceMemory memory;
    if (vkAllocateMemory(vk->device, &alloc_info, NULL, &memory) != VK_SUCCESS) {
        fprintf(stderr, "[cmd_proc] Failed to allocate image memory\n");
        vkDestroyImage(vk->device, image, NULL);
        return;
    }

    vkBindImageMemory(vk->device, image, memory, 0);

    /* Create image view */
    VkImageView image_view;
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = gravity_translate_image_view_type(cmd->texture_type),
        .format = vk_format,
        .components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = gravity_get_aspect_flags(vk_format),
            .baseMipLevel = 0,
            .levelCount = img_info.mipLevels,
            .baseArrayLayer = 0,
            .layerCount = img_info.arrayLayers,
        },
    };

    if (vkCreateImageView(vk->device, &view_info, NULL, &image_view) != VK_SUCCESS) {
        fprintf(stderr, "[cmd_proc] Failed to create image view\n");
        vkFreeMemory(vk->device, memory, NULL);
        vkDestroyImage(vk->device, image, NULL);
        return;
    }

    /* Track */
    gravity_resource_entry_t* entry =
        gravity_resource_tracker_add(tracker, cmd->handle, GRAVITY_RES_TEXTURE);
    if (!entry) {
        vkDestroyImageView(vk->device, image_view, NULL);
        vkFreeMemory(vk->device, memory, NULL);
        vkDestroyImage(vk->device, image, NULL);
        return;
    }

    entry->texture.image = image;
    entry->texture.image_view = image_view;
    entry->texture.memory = memory;
    entry->texture.width = cmd->width;
    entry->texture.height = cmd->height;
    entry->texture.depth = cmd->depth;
    entry->texture.mip_levels = img_info.mipLevels;
    entry->texture.array_layers = img_info.arrayLayers;
    entry->texture.format = vk_format;
    entry->texture.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    (void)ring;  /* Will be used for initial data upload */
}

/* ═══════════════════════════════════════════════════════════════════════
 * Sampler Creation
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_create_sampler(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    const gravity_cmd_create_sampler_t* cmd)
{
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = (VkFilter)gravity_to_vk_filter(cmd->mag_filter),
        .minFilter = (VkFilter)gravity_to_vk_filter(cmd->min_filter),
        .mipmapMode = (VkSamplerMipmapMode)gravity_to_vk_mipmap_mode(cmd->mip_filter),
        .addressModeU = (VkSamplerAddressMode)gravity_to_vk_address_mode(cmd->address_mode_s),
        .addressModeV = (VkSamplerAddressMode)gravity_to_vk_address_mode(cmd->address_mode_t),
        .addressModeW = (VkSamplerAddressMode)gravity_to_vk_address_mode(cmd->address_mode_r),
        .mipLodBias = 0.0f,
        .anisotropyEnable = cmd->max_anisotropy > 1 ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = (float)cmd->max_anisotropy,
        .compareEnable = cmd->compare_function != GRAVITY_COMPARE_NEVER ? VK_TRUE : VK_FALSE,
        .compareOp = (VkCompareOp)gravity_to_vk_compare_op(cmd->compare_function),
        .minLod = cmd->lod_min_clamp,
        .maxLod = cmd->lod_max_clamp,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkSampler sampler;
    if (vkCreateSampler(vk->device, &sampler_info, NULL, &sampler) != VK_SUCCESS) {
        fprintf(stderr, "[cmd_proc] Failed to create sampler handle=0x%08x\n",
                cmd->handle);
        return;
    }

    gravity_resource_entry_t* entry =
        gravity_resource_tracker_add(tracker, cmd->handle, GRAVITY_RES_SAMPLER);
    if (!entry) {
        vkDestroySampler(vk->device, sampler, NULL);
        return;
    }

    entry->sampler.sampler = sampler;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Shader Creation (Stub — will expand with MSL→SPIR-V compiler)
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_create_shader(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    gravity_ring_t* ring,
    const gravity_cmd_create_shader_t* cmd)
{
    /*
     * TODO: This is where MSL→SPIR-V translation will happen.
     *
     * For Phase 1, we just create a placeholder entry.
     * For Phase 2, we'll use hardcoded SPIR-V for test shaders.
     * For Phase 3+, we'll implement actual MSL parsing → SPIR-V generation.
     */

    fprintf(stderr, "[cmd_proc] Shader creation stub for handle=0x%08x "
            "type=%u source_type=%u entry=%s\n",
            cmd->handle, cmd->shader_type, cmd->source_type, cmd->entry_point);

    /* Create a placeholder tracker entry */
    gravity_resource_entry_t* entry =
        gravity_resource_tracker_add(tracker, cmd->handle, GRAVITY_RES_SHADER);
    if (!entry) return;

    entry->shader.module = VK_NULL_HANDLE;  /* Will be set when compiler is implemented */
    entry->shader.shader_type = cmd->shader_type;

    (void)vk;
    (void)ring;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Depth/Stencil State Creation
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_create_depth_stencil(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    const gravity_cmd_create_depth_stencil_t* cmd)
{
    gravity_resource_entry_t* entry =
        gravity_resource_tracker_add(tracker, cmd->handle, GRAVITY_RES_DEPTH_STENCIL);
    if (!entry) return;

    entry->depth_stencil.depth_compare = cmd->depth_compare;
    entry->depth_stencil.depth_write_enabled = cmd->depth_write_enabled;
    entry->depth_stencil.front_face = cmd->front_face_stencil;
    entry->depth_stencil.back_face = cmd->back_face_stencil;

    (void)vk;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Render Pipeline Creation (Stub — needs shader modules)
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_create_render_pipeline(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    const gravity_cmd_create_render_pipeline_t* cmd)
{
    /* TODO: Full pipeline creation with VkGraphicsPipelineCreateInfo.
     * This requires functional shader modules, which need the MSL→SPIR-V
     * compiler. For now, create a placeholder. */

    fprintf(stderr, "[cmd_proc] Render pipeline creation stub for handle=0x%08x "
            "vs=0x%08x fs=0x%08x\n",
            cmd->handle, cmd->vertex_shader, cmd->fragment_shader);

    gravity_resource_entry_t* entry =
        gravity_resource_tracker_add(tracker, cmd->handle,
                                      GRAVITY_RES_RENDER_PIPELINE);
    if (!entry) return;

    entry->render_pipeline.pipeline = VK_NULL_HANDLE;
    entry->render_pipeline.layout = VK_NULL_HANDLE;
    entry->render_pipeline.desc_set_layout = VK_NULL_HANDLE;

    (void)vk;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Command Buffer Management
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_cmdbuf_create(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    const gravity_cmd_cmdbuf_create_t* cmd)
{
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk->graphics_cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd_buf;
    if (vkAllocateCommandBuffers(vk->device, &alloc_info, &cmd_buf) != VK_SUCCESS) {
        fprintf(stderr, "[cmd_proc] Failed to allocate command buffer\n");
        return;
    }

    /* Create a fence for tracking completion */
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VkFence fence;
    if (vkCreateFence(vk->device, &fence_info, NULL, &fence) != VK_SUCCESS) {
        fprintf(stderr, "[cmd_proc] Failed to create fence for cmdbuf\n");
        vkFreeCommandBuffers(vk->device, vk->graphics_cmd_pool, 1, &cmd_buf);
        return;
    }

    /* Begin recording */
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd_buf, &begin_info);

    /* Track */
    gravity_resource_entry_t* entry =
        gravity_resource_tracker_add(tracker, cmd->handle, GRAVITY_RES_CMDBUF);
    if (!entry) {
        vkDestroyFence(vk->device, fence, NULL);
        vkFreeCommandBuffers(vk->device, vk->graphics_cmd_pool, 1, &cmd_buf);
        return;
    }

    entry->cmdbuf.cmd_buf = cmd_buf;
    entry->cmdbuf.fence = fence;
    entry->cmdbuf.recording = true;
    entry->cmdbuf.in_render_pass = false;
}

void gravity_cmd_process_cmdbuf_commit(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    gravity_ring_t* ring,
    const gravity_cmd_cmdbuf_commit_t* cmd)
{
    gravity_resource_entry_t* entry =
        gravity_resource_tracker_get(tracker, cmd->handle);
    if (!entry || entry->type != GRAVITY_RES_CMDBUF) {
        fprintf(stderr, "[cmd_proc] Invalid cmdbuf handle 0x%08x for commit\n",
                cmd->handle);
        return;
    }

    /* End recording */
    vkEndCommandBuffer(entry->cmdbuf.cmd_buf);
    entry->cmdbuf.recording = false;

    /* Submit */
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &entry->cmdbuf.cmd_buf,
    };

    vkQueueSubmit(vk->graphics_queue, 1, &submit_info, entry->cmdbuf.fence);

    /* Wait for completion (synchronous for now — will be async in Phase 4) */
    vkWaitForFences(vk->device, 1, &entry->cmdbuf.fence, VK_TRUE, UINT64_MAX);

    /* Send completion notification */
    gravity_cmd_cmdbuf_completed_t comp;
    gravity_cmd_init(&comp.hdr, GRAVITY_CMD_CMDBUF_COMPLETED,
                     sizeof(comp), cmd->hdr.sequence);
    comp.handle = cmd->handle;
    comp.status = 0;  /* Success */
    comp.gpu_time_ns = 0;  /* TODO: Query timestamp */
    gravity_ring_comp_write(ring, &comp, sizeof(comp));

    /* Clean up */
    vkResetFences(vk->device, 1, &entry->cmdbuf.fence);
    vkResetCommandBuffer(entry->cmdbuf.cmd_buf, 0);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Render Encoder Processing
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_render(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    gravity_ring_t* ring,
    const gravity_cmd_header_t* hdr,
    const void* cmd_data)
{
    switch (hdr->opcode) {
    case GRAVITY_CMD_RENDER_BEGIN: {
        const gravity_cmd_render_begin_t* cmd =
            (const gravity_cmd_render_begin_t*)cmd_data;

        gravity_resource_entry_t* cmdbuf_entry =
            gravity_resource_tracker_get(tracker, cmd->cmdbuf);
        if (!cmdbuf_entry) break;

        /* Build dynamic rendering info */
        VkRenderingAttachmentInfo color_attachments[GRAVITY_MAX_COLOR_ATTACHMENTS];
        uint32_t color_count = cmd->num_color_attachments;

        for (uint32_t i = 0; i < color_count; i++) {
            const gravity_color_attachment_action_t* ca = &cmd->color_attachments[i];
            gravity_resource_entry_t* tex_entry =
                gravity_resource_tracker_get(tracker, ca->texture);

            color_attachments[i] = (VkRenderingAttachmentInfo){
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = tex_entry ? tex_entry->texture.image_view : VK_NULL_HANDLE,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = (VkAttachmentLoadOp)gravity_to_vk_load_op(ca->load_action),
                .storeOp = (VkAttachmentStoreOp)gravity_to_vk_store_op(ca->store_action),
                .clearValue.color = {{
                    ca->clear_color[0], ca->clear_color[1],
                    ca->clear_color[2], ca->clear_color[3]
                }},
            };

            /* Transition image layout if needed */
            if (tex_entry &&
                tex_entry->texture.current_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
                VkImageMemoryBarrier barrier = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .oldLayout = tex_entry->texture.current_layout,
                    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = tex_entry->texture.image,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = ca->level,
                        .levelCount = 1,
                        .baseArrayLayer = ca->slice,
                        .layerCount = 1,
                    },
                    .srcAccessMask = 0,
                    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                };

                vkCmdPipelineBarrier(cmdbuf_entry->cmdbuf.cmd_buf,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    0, 0, NULL, 0, NULL, 1, &barrier);

                tex_entry->texture.current_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }
        }

        VkRenderingInfo rendering_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {
                .offset = { (int32_t)cmd->render_area_x, (int32_t)cmd->render_area_y },
                .extent = { cmd->render_area_width, cmd->render_area_height },
            },
            .layerCount = 1,
            .colorAttachmentCount = color_count,
            .pColorAttachments = color_attachments,
        };

        /* Depth attachment */
        VkRenderingAttachmentInfo depth_attachment = { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        if (cmd->depth_attachment.texture != GRAVITY_INVALID_HANDLE) {
            gravity_resource_entry_t* depth_tex =
                gravity_resource_tracker_get(tracker, cmd->depth_attachment.texture);
            if (depth_tex) {
                depth_attachment.imageView = depth_tex->texture.image_view;
                depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depth_attachment.loadOp = (VkAttachmentLoadOp)gravity_to_vk_load_op(cmd->depth_attachment.load_action);
                depth_attachment.storeOp = (VkAttachmentStoreOp)gravity_to_vk_store_op(cmd->depth_attachment.store_action);
                depth_attachment.clearValue.depthStencil.depth = cmd->depth_attachment.clear_depth;
                rendering_info.pDepthAttachment = &depth_attachment;
            }
        }

        vkCmdBeginRendering(cmdbuf_entry->cmdbuf.cmd_buf, &rendering_info);
        cmdbuf_entry->cmdbuf.in_render_pass = true;
        break;
    }

    case GRAVITY_CMD_RENDER_END: {
        const gravity_cmd_render_end_t* cmd =
            (const gravity_cmd_render_end_t*)cmd_data;
        gravity_resource_entry_t* cmdbuf_entry =
            gravity_resource_tracker_get(tracker, cmd->cmdbuf);
        if (cmdbuf_entry && cmdbuf_entry->cmdbuf.in_render_pass) {
            vkCmdEndRendering(cmdbuf_entry->cmdbuf.cmd_buf);
            cmdbuf_entry->cmdbuf.in_render_pass = false;
        }
        break;
    }

    case GRAVITY_CMD_RENDER_SET_VIEWPORT: {
        const gravity_cmd_render_set_viewport_t* cmd =
            (const gravity_cmd_render_set_viewport_t*)cmd_data;
        gravity_resource_entry_t* cmdbuf_entry =
            gravity_resource_tracker_get(tracker, cmd->cmdbuf);
        if (!cmdbuf_entry) break;

        /* Metal has origin at top-left with Y pointing down.
         * Vulkan has origin at top-left with Y pointing down (with VK_KHR_maintenance1).
         * But Metal's NDC Z range is [0,1] while Vulkan is [0,1] by default in 1.3.
         * So we flip Y for compatibility and adjust Z. */
        VkViewport viewport = {
            .x = cmd->viewport.x,
            .y = cmd->viewport.y + cmd->viewport.height,  /* Flip Y */
            .width = cmd->viewport.width,
            .height = -cmd->viewport.height,                /* Negative height = flip */
            .minDepth = cmd->viewport.znear,
            .maxDepth = cmd->viewport.zfar,
        };
        vkCmdSetViewport(cmdbuf_entry->cmdbuf.cmd_buf, 0, 1, &viewport);
        break;
    }

    case GRAVITY_CMD_RENDER_SET_SCISSOR: {
        const gravity_cmd_render_set_scissor_t* cmd =
            (const gravity_cmd_render_set_scissor_t*)cmd_data;
        gravity_resource_entry_t* cmdbuf_entry =
            gravity_resource_tracker_get(tracker, cmd->cmdbuf);
        if (!cmdbuf_entry) break;

        VkRect2D scissor = {
            .offset = { (int32_t)cmd->rect.x, (int32_t)cmd->rect.y },
            .extent = { cmd->rect.width, cmd->rect.height },
        };
        vkCmdSetScissor(cmdbuf_entry->cmdbuf.cmd_buf, 0, 1, &scissor);
        break;
    }

    case GRAVITY_CMD_RENDER_SET_VERTEX_BUFFER: {
        const gravity_cmd_render_set_vertex_buffer_t* cmd =
            (const gravity_cmd_render_set_vertex_buffer_t*)cmd_data;
        gravity_resource_entry_t* cmdbuf_entry =
            gravity_resource_tracker_get(tracker, cmd->cmdbuf);
        gravity_resource_entry_t* buf_entry =
            gravity_resource_tracker_get(tracker, cmd->buffer);
        if (!cmdbuf_entry || !buf_entry) break;

        VkDeviceSize offset = cmd->offset;
        vkCmdBindVertexBuffers(cmdbuf_entry->cmdbuf.cmd_buf,
                               cmd->index, 1, &buf_entry->buffer.buffer, &offset);
        break;
    }

    case GRAVITY_CMD_RENDER_DRAW: {
        const gravity_cmd_render_draw_t* cmd =
            (const gravity_cmd_render_draw_t*)cmd_data;
        gravity_resource_entry_t* cmdbuf_entry =
            gravity_resource_tracker_get(tracker, cmd->cmdbuf);
        if (!cmdbuf_entry) break;

        vkCmdDraw(cmdbuf_entry->cmdbuf.cmd_buf,
                  cmd->vertex_count, 1, cmd->vertex_start, 0);
        break;
    }

    case GRAVITY_CMD_RENDER_DRAW_INDEXED: {
        const gravity_cmd_render_draw_indexed_t* cmd =
            (const gravity_cmd_render_draw_indexed_t*)cmd_data;
        gravity_resource_entry_t* cmdbuf_entry =
            gravity_resource_tracker_get(tracker, cmd->cmdbuf);
        gravity_resource_entry_t* idx_entry =
            gravity_resource_tracker_get(tracker, cmd->index_buffer);
        if (!cmdbuf_entry || !idx_entry) break;

        VkIndexType idx_type = (VkIndexType)gravity_to_vk_index_type(cmd->index_type);
        vkCmdBindIndexBuffer(cmdbuf_entry->cmdbuf.cmd_buf,
                             idx_entry->buffer.buffer,
                             cmd->index_buffer_offset, idx_type);
        vkCmdDrawIndexed(cmdbuf_entry->cmdbuf.cmd_buf,
                         cmd->index_count, 1, 0, 0, 0);
        break;
    }

    case GRAVITY_CMD_RENDER_DRAW_INSTANCED: {
        const gravity_cmd_render_draw_instanced_t* cmd =
            (const gravity_cmd_render_draw_instanced_t*)cmd_data;
        gravity_resource_entry_t* cmdbuf_entry =
            gravity_resource_tracker_get(tracker, cmd->cmdbuf);
        if (!cmdbuf_entry) break;

        vkCmdDraw(cmdbuf_entry->cmdbuf.cmd_buf,
                  cmd->vertex_count, cmd->instance_count,
                  cmd->vertex_start, cmd->base_instance);
        break;
    }

    default:
        /* Other render commands will be handled in Phase 3+ */
        break;
    }

    (void)ring;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Compute Encoder Processing (Stub)
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_compute(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    gravity_ring_t* ring,
    const gravity_cmd_header_t* hdr,
    const void* cmd_data)
{
    /* TODO: Implement compute pipeline translation in Phase 4 */
    fprintf(stderr, "[cmd_proc] Compute command 0x%04x not yet implemented\n",
            hdr->opcode);
    (void)vk; (void)tracker; (void)ring; (void)cmd_data;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Blit Encoder Processing (Stub)
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_blit(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    gravity_ring_t* ring,
    const gravity_cmd_header_t* hdr,
    const void* cmd_data)
{
    /* TODO: Implement blit operations in Phase 3+ */
    fprintf(stderr, "[cmd_proc] Blit command 0x%04x not yet implemented\n",
            hdr->opcode);
    (void)vk; (void)tracker; (void)ring; (void)cmd_data;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Presentation
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_cmd_process_present(
    gravity_vk_state_t* vk,
    gravity_resource_tracker_t* tracker,
    gravity_ring_t* ring,
    const gravity_cmd_present_t* cmd)
{
    /* TODO: Present the rendered frame.
     * Options:
     * 1. Copy to QEMU's standard framebuffer (via VGA device memory)
     * 2. Render to a separate host window (via VkSwapchain)
     * 3. Pipe to a virtual display via DMA-BUF
     */
    fprintf(stderr, "[cmd_proc] Present texture=0x%08x (stub)\n",
            cmd->texture);
    (void)vk; (void)tracker; (void)ring;
}

void gravity_cmd_process_resize_display(
    gravity_vk_state_t* vk,
    const gravity_cmd_resize_display_t* cmd)
{
    /* TODO: Resize framebuffer / swapchain */
    fprintf(stderr, "[cmd_proc] Resize display to %ux%u (stub)\n",
            cmd->width, cmd->height);
    (void)vk;
}
