/*
 * test_ring_buffer.c — Ring Buffer Unit Tests
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "gravity_ring.h"

#define SHMEM_SIZE (10 * 1024 * 1024)
#define CMD_SIZE (4 * 1024 * 1024)
#define COMP_SIZE (1 * 1024 * 1024)

static void test_ring_init_attach()
{
    void* shmem = malloc(SHMEM_SIZE);
    assert(shmem != NULL);

    int ret = gravity_ring_init_host(shmem, SHMEM_SIZE, CMD_SIZE, COMP_SIZE);
    assert(ret == 0);

    gravity_ring_t ring;
    ret = gravity_ring_attach(&ring, shmem);
    assert(ret == 0);

    assert(ring.cmd_ring_size == CMD_SIZE);
    assert(ring.comp_ring_size == COMP_SIZE);
    assert(ring.data_region_size == SHMEM_SIZE - sizeof(gravity_ring_header_t) - CMD_SIZE - COMP_SIZE);

    free(shmem);
    printf("PASS: test_ring_init_attach\n");
}

static void test_ring_read_write()
{
    void* shmem = malloc(SHMEM_SIZE);
    gravity_ring_init_host(shmem, SHMEM_SIZE, CMD_SIZE, COMP_SIZE);

    gravity_ring_t ring;
    gravity_ring_attach(&ring, shmem);

    uint32_t test_data[] = { 0xDEADBEEF, 0xCAFEBABE, 0x12345678 };
    int ret = gravity_ring_cmd_write(&ring, test_data, sizeof(test_data));
    assert(ret == 0);

    uint32_t read_buf[3];
    const void* peeked = gravity_ring_cmd_peek(&ring, read_buf, sizeof(read_buf));
    assert(peeked != NULL);
    assert(memcmp(read_buf, test_data, sizeof(test_data)) == 0);

    gravity_ring_cmd_consume(&ring, sizeof(test_data));

    // Try peek again, should be empty
    peeked = gravity_ring_cmd_peek(&ring, read_buf, sizeof(read_buf));
    assert(peeked == NULL);

    free(shmem);
    printf("PASS: test_ring_read_write\n");
}

int main()
{
    printf("Running Ring Buffer Tests...\n");
    test_ring_init_attach();
    test_ring_read_write();
    printf("All Ring Buffer Tests Passed!\n");
    return 0;
}
