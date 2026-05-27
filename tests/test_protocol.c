/*
 * test_protocol.c — Protocol Unit Tests
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "gravity_protocol.h"
#include "gravity_formats.h"

static void test_command_init()
{
    gravity_cmd_header_t hdr;
    gravity_cmd_init(&hdr, GRAVITY_CMD_HELLO, sizeof(hdr), 42);

    assert(hdr.opcode == GRAVITY_CMD_HELLO);
    assert(hdr.size == sizeof(hdr));
    assert(hdr.sequence == 42);

    printf("PASS: test_command_init\n");
}

static void test_command_validation()
{
    gravity_cmd_header_t hdr;
    gravity_cmd_init(&hdr, GRAVITY_CMD_HELLO, sizeof(hdr), 1);

    assert(gravity_cmd_validate(&hdr, sizeof(hdr)) == 0);
    assert(gravity_cmd_validate(&hdr, sizeof(hdr) - 1) == -1); // Buffer too small
    assert(gravity_cmd_validate(&hdr, sizeof(gravity_cmd_header_t) - 1) == -1);

    hdr.opcode = GRAVITY_CMD_MAX;
    assert(gravity_cmd_validate(&hdr, sizeof(hdr)) == -1); // Invalid opcode

    printf("PASS: test_command_validation\n");
}

static void test_format_mapping()
{
    assert(gravity_metal_to_vk_pixel_format(GRAVITY_PIXEL_FORMAT_RGBA8_UNORM) == 37); // VK_FORMAT_R8G8B8A8_UNORM
    assert(gravity_metal_to_vk_vertex_format(GRAVITY_VERTEX_FORMAT_FLOAT3) == 106); // VK_FORMAT_R32G32B32_SFLOAT

    assert(gravity_pixel_format_is_depth_stencil(GRAVITY_PIXEL_FORMAT_DEPTH32_FLOAT) == 1);
    assert(gravity_pixel_format_is_depth_stencil(GRAVITY_PIXEL_FORMAT_RGBA8_UNORM) == 0);

    printf("PASS: test_format_mapping\n");
}

int main()
{
    printf("Running Protocol Tests...\n");
    test_command_init();
    test_command_validation();
    test_format_mapping();
    printf("All Protocol Tests Passed!\n");
    return 0;
}
