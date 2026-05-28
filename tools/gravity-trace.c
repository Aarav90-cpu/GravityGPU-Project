/*
 * gravity-trace.c — GravityGPU Debug Trace Tool
 *
 * Connects to the shared memory region and dumps commands.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "gravity_protocol.h"
#include "gravity_ring.h"

int main(int argc, char **argv) {
  const char *shmem_path = "/dev/shm/gravity-gpu";
  if (argc > 1) {
    shmem_path = argv[1];
  }

  int fd = open(shmem_path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Failed to open %s: %s\n", shmem_path, strerror(errno));
    return 1;
  }

  off_t size = lseek(fd, 0, SEEK_END);
  if (size <= 0) {
    fprintf(stderr, "Invalid shared memory size\n");
    return 1;
  }

  void *shmem = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);

  if (shmem == MAP_FAILED) {
    fprintf(stderr, "Failed to mmap %s: %s\n", shmem_path, strerror(errno));
    return 1;
  }

  gravity_ring_header_t *hdr = (gravity_ring_header_t *)shmem;
  if (hdr->magic != GRAVITY_PROTOCOL_MAGIC) {
    fprintf(stderr, "Invalid magic number in shared memory\n");
    return 1;
  }

  printf("GravityGPU Trace Tool attached to %s\n", shmem_path);
  printf("Host Protocol: %u.%u\n", hdr->version >> 16, hdr->version & 0xFFFF);
  printf("Command Ring: head=%u tail=%u size=%u\n", hdr->cmd_head,
         hdr->cmd_tail, hdr->cmd_ring_size);
  printf("Completion Ring: head=%u tail=%u size=%u\n", hdr->comp_head,
         hdr->comp_tail, hdr->comp_ring_size);

  munmap(shmem, size);
  return 0;
}
