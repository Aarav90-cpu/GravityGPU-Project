/*
 * gravity_protocol.c — Wire Protocol Helpers
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include "gravity_protocol.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Command Name Table
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint16_t    opcode;
    const char* name;
} gravity_cmd_name_entry_t;

static const gravity_cmd_name_entry_t cmd_names[] = {
    { GRAVITY_CMD_NOP,                      "NOP" },
    { GRAVITY_CMD_HELLO,                    "HELLO" },
    { GRAVITY_CMD_GOODBYE,                  "GOODBYE" },
    { GRAVITY_CMD_PING,                     "PING" },
    { GRAVITY_CMD_PONG,                     "PONG" },
    { GRAVITY_CMD_GET_DEVICE_INFO,          "GET_DEVICE_INFO" },
    { GRAVITY_CMD_DEVICE_INFO_RESPONSE,     "DEVICE_INFO_RESPONSE" },
    { GRAVITY_CMD_CREATE_BUFFER,            "CREATE_BUFFER" },
    { GRAVITY_CMD_DESTROY_BUFFER,           "DESTROY_BUFFER" },
    { GRAVITY_CMD_CREATE_TEXTURE,           "CREATE_TEXTURE" },
    { GRAVITY_CMD_DESTROY_TEXTURE,          "DESTROY_TEXTURE" },
    { GRAVITY_CMD_CREATE_SAMPLER,           "CREATE_SAMPLER" },
    { GRAVITY_CMD_DESTROY_SAMPLER,          "DESTROY_SAMPLER" },
    { GRAVITY_CMD_CREATE_SHADER,            "CREATE_SHADER" },
    { GRAVITY_CMD_DESTROY_SHADER,           "DESTROY_SHADER" },
    { GRAVITY_CMD_CREATE_RENDER_PIPELINE,   "CREATE_RENDER_PIPELINE" },
    { GRAVITY_CMD_DESTROY_RENDER_PIPELINE,  "DESTROY_RENDER_PIPELINE" },
    { GRAVITY_CMD_CREATE_COMPUTE_PIPELINE,  "CREATE_COMPUTE_PIPELINE" },
    { GRAVITY_CMD_DESTROY_COMPUTE_PIPELINE, "DESTROY_COMPUTE_PIPELINE" },
    { GRAVITY_CMD_CREATE_DEPTH_STENCIL,     "CREATE_DEPTH_STENCIL" },
    { GRAVITY_CMD_DESTROY_DEPTH_STENCIL,    "DESTROY_DEPTH_STENCIL" },
    { GRAVITY_CMD_CMDBUF_CREATE,            "CMDBUF_CREATE" },
    { GRAVITY_CMD_CMDBUF_COMMIT,            "CMDBUF_COMMIT" },
    { GRAVITY_CMD_CMDBUF_WAIT,              "CMDBUF_WAIT" },
    { GRAVITY_CMD_CMDBUF_COMPLETED,         "CMDBUF_COMPLETED" },
    { GRAVITY_CMD_RENDER_BEGIN,             "RENDER_BEGIN" },
    { GRAVITY_CMD_RENDER_END,               "RENDER_END" },
    { GRAVITY_CMD_RENDER_SET_PIPELINE,      "RENDER_SET_PIPELINE" },
    { GRAVITY_CMD_RENDER_SET_VERTEX_BUFFER, "RENDER_SET_VERTEX_BUFFER" },
    { GRAVITY_CMD_RENDER_SET_FRAGMENT_BUFFER, "RENDER_SET_FRAGMENT_BUFFER" },
    { GRAVITY_CMD_RENDER_SET_FRAGMENT_TEXTURE, "RENDER_SET_FRAGMENT_TEXTURE" },
    { GRAVITY_CMD_RENDER_SET_FRAGMENT_SAMPLER, "RENDER_SET_FRAGMENT_SAMPLER" },
    { GRAVITY_CMD_RENDER_SET_VERTEX_TEXTURE, "RENDER_SET_VERTEX_TEXTURE" },
    { GRAVITY_CMD_RENDER_SET_VERTEX_SAMPLER, "RENDER_SET_VERTEX_SAMPLER" },
    { GRAVITY_CMD_RENDER_SET_VIEWPORT,      "RENDER_SET_VIEWPORT" },
    { GRAVITY_CMD_RENDER_SET_SCISSOR,       "RENDER_SET_SCISSOR" },
    { GRAVITY_CMD_RENDER_SET_CULL_MODE,     "RENDER_SET_CULL_MODE" },
    { GRAVITY_CMD_RENDER_SET_WINDING,       "RENDER_SET_WINDING" },
    { GRAVITY_CMD_RENDER_SET_DEPTH_STENCIL, "RENDER_SET_DEPTH_STENCIL" },
    { GRAVITY_CMD_RENDER_SET_DEPTH_BIAS,    "RENDER_SET_DEPTH_BIAS" },
    { GRAVITY_CMD_RENDER_SET_STENCIL_REF,   "RENDER_SET_STENCIL_REF" },
    { GRAVITY_CMD_RENDER_SET_BLEND_COLOR,   "RENDER_SET_BLEND_COLOR" },
    { GRAVITY_CMD_RENDER_SET_TRIANGLE_FILL, "RENDER_SET_TRIANGLE_FILL" },
    { GRAVITY_CMD_RENDER_DRAW,              "RENDER_DRAW" },
    { GRAVITY_CMD_RENDER_DRAW_INDEXED,      "RENDER_DRAW_INDEXED" },
    { GRAVITY_CMD_RENDER_DRAW_INSTANCED,    "RENDER_DRAW_INSTANCED" },
    { GRAVITY_CMD_RENDER_DRAW_INDEXED_INSTANCED, "RENDER_DRAW_INDEXED_INSTANCED" },
    { GRAVITY_CMD_RENDER_DRAW_INDIRECT,     "RENDER_DRAW_INDIRECT" },
    { GRAVITY_CMD_COMPUTE_BEGIN,            "COMPUTE_BEGIN" },
    { GRAVITY_CMD_COMPUTE_END,              "COMPUTE_END" },
    { GRAVITY_CMD_COMPUTE_SET_PIPELINE,     "COMPUTE_SET_PIPELINE" },
    { GRAVITY_CMD_COMPUTE_SET_BUFFER,       "COMPUTE_SET_BUFFER" },
    { GRAVITY_CMD_COMPUTE_SET_TEXTURE,      "COMPUTE_SET_TEXTURE" },
    { GRAVITY_CMD_COMPUTE_SET_SAMPLER,      "COMPUTE_SET_SAMPLER" },
    { GRAVITY_CMD_COMPUTE_DISPATCH,         "COMPUTE_DISPATCH" },
    { GRAVITY_CMD_COMPUTE_DISPATCH_INDIRECT, "COMPUTE_DISPATCH_INDIRECT" },
    { GRAVITY_CMD_BLIT_BEGIN,               "BLIT_BEGIN" },
    { GRAVITY_CMD_BLIT_END,                 "BLIT_END" },
    { GRAVITY_CMD_BLIT_COPY_BUFFER,         "BLIT_COPY_BUFFER" },
    { GRAVITY_CMD_BLIT_COPY_TEXTURE,        "BLIT_COPY_TEXTURE" },
    { GRAVITY_CMD_BLIT_COPY_BUF_TO_TEX,    "BLIT_COPY_BUF_TO_TEX" },
    { GRAVITY_CMD_BLIT_COPY_TEX_TO_BUF,    "BLIT_COPY_TEX_TO_BUF" },
    { GRAVITY_CMD_BLIT_GENERATE_MIPMAPS,    "BLIT_GENERATE_MIPMAPS" },
    { GRAVITY_CMD_BLIT_FILL_BUFFER,         "BLIT_FILL_BUFFER" },
    { GRAVITY_CMD_BLIT_SYNCHRONIZE,         "BLIT_SYNCHRONIZE" },
    { GRAVITY_CMD_CREATE_FENCE,             "CREATE_FENCE" },
    { GRAVITY_CMD_DESTROY_FENCE,            "DESTROY_FENCE" },
    { GRAVITY_CMD_WAIT_FENCE,               "WAIT_FENCE" },
    { GRAVITY_CMD_SIGNAL_FENCE,             "SIGNAL_FENCE" },
    { GRAVITY_CMD_CREATE_EVENT,             "CREATE_EVENT" },
    { GRAVITY_CMD_DESTROY_EVENT,            "DESTROY_EVENT" },
    { GRAVITY_CMD_UPLOAD_DATA,              "UPLOAD_DATA" },
    { GRAVITY_CMD_READBACK_DATA,            "READBACK_DATA" },
    { GRAVITY_CMD_PRESENT,                  "PRESENT" },
    { GRAVITY_CMD_RESIZE_DISPLAY,           "RESIZE_DISPLAY" },
};

#define CMD_NAME_COUNT (sizeof(cmd_names) / sizeof(cmd_names[0]))

const char* gravity_cmd_name(uint16_t opcode)
{
    for (size_t i = 0; i < CMD_NAME_COUNT; i++) {
        if (cmd_names[i].opcode == opcode) {
            return cmd_names[i].name;
        }
    }
    return "UNKNOWN";
}

/* ═══════════════════════════════════════════════════════════════════════
 * Command Validation
 * ═══════════════════════════════════════════════════════════════════════ */

int gravity_cmd_validate(const gravity_cmd_header_t* hdr, size_t available_bytes)
{
    if (!hdr) return -1;
    if (available_bytes < sizeof(gravity_cmd_header_t)) return -1;
    if (hdr->size < sizeof(gravity_cmd_header_t)) return -1;
    if (hdr->size > available_bytes) return -1;
    if (hdr->opcode >= GRAVITY_CMD_MAX) return -1;
    return 0;
}
