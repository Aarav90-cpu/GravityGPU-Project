/*
 * gravity_ring.h — Lock-Free Ring Buffer for GPU Command Transport
 *
 * Implements a single-producer single-consumer (SPSC) ring buffer
 * for passing serialized Metal commands from guest to host across
 * the QEMU shared memory boundary.
 *
 * Memory layout in shared memory (BAR2):
 *
 *   +------------------+ offset 0
 *   | Ring Header      |   64 bytes (producer/consumer indices, status)
 *   +------------------+ offset 64
 *   | Command Ring     |   (ring_size - 64 - completion_size) bytes
 *   +------------------+
 *   | Completion Ring  |   completion_size bytes (host → guest responses)
 *   +------------------+
 *   | Data Region      |   Remaining shared memory for bulk data
 *   +------------------+
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#ifndef GRAVITY_RING_H
#define GRAVITY_RING_H

#include <stdint.h>
#include <stddef.h>
#ifdef KERNEL
#ifdef __cplusplus
extern "C" {
#endif
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
#ifdef __cplusplus
}
#endif
#else
#include <string.h>
#endif
#include "gravity_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════
 * Memory Barriers — x86-64 specific (both guest and host are x86)
 * ═══════════════════════════════════════════════════════════════════════ */

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
    /* x86 has strong memory ordering; only compiler barriers needed
     * for store-load. We use explicit fences for the producer/consumer
     * pattern to be safe across hypervisor boundary. */
    #define gravity_mb()        __asm__ volatile("mfence" ::: "memory")
    #define gravity_rmb()       __asm__ volatile("lfence" ::: "memory")
    #define gravity_wmb()       __asm__ volatile("sfence" ::: "memory")
    #define gravity_compiler_barrier() __asm__ volatile("" ::: "memory")
#elif defined(__aarch64__)
    /* ARM64 (shouldn't be needed for x86 KVM, but here for completeness) */
    #define gravity_mb()        __asm__ volatile("dmb sy" ::: "memory")
    #define gravity_rmb()       __asm__ volatile("dmb ld" ::: "memory")
    #define gravity_wmb()       __asm__ volatile("dmb st" ::: "memory")
    #define gravity_compiler_barrier() __asm__ volatile("" ::: "memory")
#else
    /* Fallback: use GCC/Clang builtins */
    #define gravity_mb()        __sync_synchronize()
    #define gravity_rmb()       __sync_synchronize()
    #define gravity_wmb()       __sync_synchronize()
    #define gravity_compiler_barrier() __asm__ volatile("" ::: "memory")
#endif

/* Atomic load/store for indices */
#define gravity_load_acquire(ptr)   __atomic_load_n(ptr, __ATOMIC_ACQUIRE)
#define gravity_store_release(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_RELEASE)

/* ═══════════════════════════════════════════════════════════════════════
 * Ring Header — Lives at offset 0 in shared memory
 * ═══════════════════════════════════════════════════════════════════════ */

#define GRAVITY_RING_MAGIC          0x52494E47  /* "RING" */
#define GRAVITY_RING_HEADER_SIZE    64
#define GRAVITY_RING_ALIGN          64          /* Cache line alignment */

/* Status bits */
#define GRAVITY_RING_STATUS_READY   0x0001
#define GRAVITY_RING_STATUS_ERROR   0x8000

#pragma pack(push, 1)

typedef struct gravity_ring_header {
    /* Immutable after init (set by host, read by guest) */
    uint32_t    magic;              /* GRAVITY_RING_MAGIC */
    uint32_t    version;            /* Protocol version */
    uint32_t    cmd_ring_offset;    /* Offset from base to command ring */
    uint32_t    cmd_ring_size;      /* Size of command ring in bytes (power of 2) */
    uint32_t    comp_ring_offset;   /* Offset from base to completion ring */
    uint32_t    comp_ring_size;     /* Size of completion ring in bytes */
    uint32_t    data_region_offset; /* Offset from base to data region */
    uint32_t    data_region_size;   /* Size of data region */

    /* Command ring indices (guest produces, host consumes) */
    volatile uint32_t   cmd_head;   /* Written by host (consumer) */
    volatile uint32_t   cmd_tail;   /* Written by guest (producer) */

    /* Completion ring indices (host produces, guest consumes) */
    volatile uint32_t   comp_head;  /* Written by guest (consumer) */
    volatile uint32_t   comp_tail;  /* Written by host (producer) */

    /* Status */
    volatile uint32_t   status;     /* GRAVITY_RING_STATUS_* */
    uint32_t            reserved[3];
} gravity_ring_header_t;

_Static_assert(sizeof(gravity_ring_header_t) == GRAVITY_RING_HEADER_SIZE,
               "ring_header must be 64 bytes");

#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════
 * Ring Buffer Context (per-side view)
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct gravity_ring {
    /* Pointers into the shared memory region */
    gravity_ring_header_t*  header;
    uint8_t*                cmd_ring;       /* Command ring base */
    uint8_t*                comp_ring;      /* Completion ring base */
    uint8_t*                data_region;    /* Data region base */

    /* Cached sizes (from header, for fast access) */
    uint32_t    cmd_ring_size;      /* Must be power of 2 */
    uint32_t    cmd_ring_mask;      /* cmd_ring_size - 1 */
    uint32_t    comp_ring_size;
    uint32_t    comp_ring_mask;
    uint32_t    data_region_size;

    /* Data region allocator (simple bump allocator, wraps around) */
    uint32_t    data_alloc_offset;
} gravity_ring_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Initialize ring header in shared memory (called by host/QEMU device).
 *
 * @param base          Pointer to the start of shared memory region
 * @param total_size    Total size of shared memory
 * @param cmd_ring_size Desired command ring size (will be rounded to power of 2)
 * @param comp_ring_size Desired completion ring size
 * @return 0 on success, -1 on error
 */
static inline int gravity_ring_init_host(void* base, uint32_t total_size,
                                          uint32_t cmd_ring_size,
                                          uint32_t comp_ring_size)
{
    gravity_ring_header_t* hdr = (gravity_ring_header_t*)base;

    /* Round cmd_ring_size up to next power of 2 */
    uint32_t ring_sz = 1;
    while (ring_sz < cmd_ring_size) ring_sz <<= 1;
    cmd_ring_size = ring_sz;

    /* Compute comp_ring_size (also power of 2) */
    ring_sz = 1;
    while (ring_sz < comp_ring_size) ring_sz <<= 1;
    comp_ring_size = ring_sz;

    uint32_t cmd_offset = GRAVITY_RING_HEADER_SIZE;
    uint32_t comp_offset = cmd_offset + cmd_ring_size;
    uint32_t data_offset = comp_offset + comp_ring_size;

    if (data_offset >= total_size) {
        return -1;  /* Not enough space */
    }

    memset(base, 0, total_size);

    hdr->magic              = GRAVITY_RING_MAGIC;
    hdr->version            = (GRAVITY_PROTOCOL_VERSION_MAJOR << 16) |
                               GRAVITY_PROTOCOL_VERSION_MINOR;
    hdr->cmd_ring_offset    = cmd_offset;
    hdr->cmd_ring_size      = cmd_ring_size;
    hdr->comp_ring_offset   = comp_offset;
    hdr->comp_ring_size     = comp_ring_size;
    hdr->data_region_offset = data_offset;
    hdr->data_region_size   = total_size - data_offset;
    hdr->cmd_head           = 0;
    hdr->cmd_tail           = 0;
    hdr->comp_head          = 0;
    hdr->comp_tail          = 0;
    hdr->status             = GRAVITY_RING_STATUS_READY;

    gravity_wmb();
    return 0;
}

/**
 * Attach to an existing ring buffer in shared memory.
 * Called by both guest (after PCI BAR mapping) and host daemon.
 *
 * @param ring  Ring context to initialize
 * @param base  Pointer to start of shared memory
 * @return 0 on success, -1 on error
 */
static inline int gravity_ring_attach(gravity_ring_t* ring, void* base)
{
    gravity_ring_header_t* hdr = (gravity_ring_header_t*)base;

    gravity_rmb();

    if (hdr->magic != GRAVITY_RING_MAGIC) {
        return -1;
    }

    if (!(hdr->status & GRAVITY_RING_STATUS_READY)) {
        return -1;
    }

    /* Validate power-of-2 sizes */
    if ((hdr->cmd_ring_size & (hdr->cmd_ring_size - 1)) != 0) {
        return -1;
    }

    ring->header          = hdr;
    ring->cmd_ring        = (uint8_t*)base + hdr->cmd_ring_offset;
    ring->comp_ring       = (uint8_t*)base + hdr->comp_ring_offset;
    ring->data_region     = (uint8_t*)base + hdr->data_region_offset;
    ring->cmd_ring_size   = hdr->cmd_ring_size;
    ring->cmd_ring_mask   = hdr->cmd_ring_size - 1;
    ring->comp_ring_size  = hdr->comp_ring_size;
    ring->comp_ring_mask  = hdr->comp_ring_size - 1;
    ring->data_region_size = hdr->data_region_size;
    ring->data_alloc_offset = 0;

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Command Ring — Producer (Guest) Operations
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Get available space in the command ring for writing.
 */
static inline uint32_t gravity_ring_cmd_space(const gravity_ring_t* ring)
{
    uint32_t head = gravity_load_acquire(&ring->header->cmd_head);
    uint32_t tail = ring->header->cmd_tail;
    /* One slot wasted to distinguish full from empty */
    return (ring->cmd_ring_size - 1) - ((tail - head) & ring->cmd_ring_mask);
}

/**
 * Write a command to the command ring (producer / guest side).
 *
 * @param ring      Ring context
 * @param data      Command data to write
 * @param size      Size of command data (must include header)
 * @return 0 on success, -1 if ring is full
 */
static inline int gravity_ring_cmd_write(gravity_ring_t* ring,
                                          const void* data, uint32_t size)
{
    if (size == 0 || size > ring->cmd_ring_size / 2) {
        return -1;  /* Sanity check */
    }

    if (gravity_ring_cmd_space(ring) < size) {
        return -1;  /* Ring full */
    }

    uint32_t tail = ring->header->cmd_tail;
    uint32_t mask = ring->cmd_ring_mask;
    const uint8_t* src = (const uint8_t*)data;

    /* Copy data, handling wraparound */
    for (uint32_t i = 0; i < size; i++) {
        ring->cmd_ring[(tail + i) & mask] = src[i];
    }

    gravity_wmb();  /* Ensure data is visible before updating tail */
    gravity_store_release(&ring->header->cmd_tail, (tail + size) & mask);

    return 0;
}

/**
 * Check if commands are available to read (consumer / host side).
 */
static inline uint32_t gravity_ring_cmd_available(const gravity_ring_t* ring)
{
    uint32_t tail = gravity_load_acquire(&ring->header->cmd_tail);
    uint32_t head = ring->header->cmd_head;
    return (tail - head) & ring->cmd_ring_mask;
}

/**
 * Peek at the next command header without consuming it (host side).
 * Returns NULL if no command available.
 */
static inline const void* gravity_ring_cmd_peek(gravity_ring_t* ring,
                                                  void* out_buf,
                                                  uint32_t buf_size)
{
    uint32_t avail = gravity_ring_cmd_available(ring);
    if (avail < sizeof(gravity_cmd_header_t)) {
        return NULL;
    }

    /* Read the header first to get command size */
    uint32_t head = ring->header->cmd_head;
    uint32_t mask = ring->cmd_ring_mask;
    uint8_t* dst = (uint8_t*)out_buf;

    /* Copy header bytes with wraparound */
    for (uint32_t i = 0; i < sizeof(gravity_cmd_header_t) && i < buf_size; i++) {
        dst[i] = ring->cmd_ring[(head + i) & mask];
    }

    gravity_rmb();

    gravity_cmd_header_t* hdr = (gravity_cmd_header_t*)out_buf;

    /* Validate and read full command */
    if (hdr->size > avail || hdr->size > buf_size) {
        return NULL;  /* Incomplete or buffer too small */
    }

    for (uint32_t i = sizeof(gravity_cmd_header_t); i < hdr->size; i++) {
        dst[i] = ring->cmd_ring[(head + i) & mask];
    }

    gravity_rmb();
    return out_buf;
}

/**
 * Consume (advance past) the last peeked command (host side).
 */
static inline void gravity_ring_cmd_consume(gravity_ring_t* ring, uint32_t size)
{
    uint32_t head = ring->header->cmd_head;
    gravity_store_release(&ring->header->cmd_head, (head + size) & ring->cmd_ring_mask);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Completion Ring — Producer (Host) Operations
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Write a completion to the completion ring (host side).
 */
static inline int gravity_ring_comp_write(gravity_ring_t* ring,
                                            const void* data, uint32_t size)
{
    uint32_t head = gravity_load_acquire(&ring->header->comp_head);
    uint32_t tail = ring->header->comp_tail;
    uint32_t space = (ring->comp_ring_size - 1) -
                     ((tail - head) & ring->comp_ring_mask);

    if (space < size) {
        return -1;
    }

    uint32_t mask = ring->comp_ring_mask;
    const uint8_t* src = (const uint8_t*)data;

    for (uint32_t i = 0; i < size; i++) {
        ring->comp_ring[(tail + i) & mask] = src[i];
    }

    gravity_wmb();
    gravity_store_release(&ring->header->comp_tail, (tail + size) & mask);

    return 0;
}

/**
 * Read a completion from the completion ring (guest side).
 */
static inline int gravity_ring_comp_read(gravity_ring_t* ring,
                                           void* out_buf, uint32_t buf_size)
{
    uint32_t tail = gravity_load_acquire(&ring->header->comp_tail);
    uint32_t head = ring->header->comp_head;
    uint32_t avail = (tail - head) & ring->comp_ring_mask;

    if (avail < sizeof(gravity_cmd_header_t)) {
        return 0;  /* No completion available */
    }

    uint32_t mask = ring->comp_ring_mask;
    uint8_t* dst = (uint8_t*)out_buf;

    /* Read header */
    for (uint32_t i = 0; i < sizeof(gravity_cmd_header_t) && i < buf_size; i++) {
        dst[i] = ring->comp_ring[(head + i) & mask];
    }

    gravity_rmb();

    gravity_cmd_header_t* hdr = (gravity_cmd_header_t*)out_buf;
    if (hdr->size > avail || hdr->size > buf_size) {
        return -1;
    }

    for (uint32_t i = sizeof(gravity_cmd_header_t); i < hdr->size; i++) {
        dst[i] = ring->comp_ring[(head + i) & mask];
    }

    gravity_rmb();
    gravity_store_release(&ring->header->comp_head, (head + hdr->size) & mask);

    return (int)hdr->size;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Data Region — Simple Bump Allocator
 * Used for passing bulk data (texture uploads, shader source, etc.)
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Allocate space in the data region and copy data there.
 *
 * @param ring      Ring context
 * @param data      Data to copy
 * @param size      Size of data
 * @param out_offset Output: offset within data region where data was placed
 * @return 0 on success, -1 if not enough space
 */
static inline int gravity_ring_data_write(gravity_ring_t* ring,
                                            const void* data, uint32_t size,
                                            uint64_t* out_offset)
{
    /* Simple wrap-around bump allocator.
     * This is not thread-safe and relies on SPSC command ordering:
     * commands referencing data offsets must be processed before
     * the data region wraps. For robust operation, the data region
     * should be large enough to hold several frames of data. */

    if (size > ring->data_region_size) {
        return -1;
    }

    uint32_t offset = ring->data_alloc_offset;

    /* Wrap if needed */
    if (offset + size > ring->data_region_size) {
        offset = 0;
    }

    memcpy(ring->data_region + offset, data, size);
    gravity_wmb();

    *out_offset = ring->header->data_region_offset + offset;

    ring->data_alloc_offset = offset + size;
    /* Align to 64 bytes for next allocation */
    ring->data_alloc_offset = (ring->data_alloc_offset + 63) & ~63u;
    if (ring->data_alloc_offset >= ring->data_region_size) {
        ring->data_alloc_offset = 0;
    }

    return 0;
}

/**
 * Get a pointer to data at a given offset within the shared memory region.
 */
static inline void* gravity_ring_data_ptr(gravity_ring_t* ring, uint64_t offset)
{
    /* offset is absolute from shared memory base.
     * data_region is at header->data_region_offset from base.
     * ring->data_region already points to the right place. */
    uint32_t data_start = ring->header->data_region_offset;
    if (offset < data_start) return NULL;

    uint32_t local_offset = (uint32_t)(offset - data_start);
    if (local_offset >= ring->data_region_size) return NULL;

    return ring->data_region + local_offset;
}

#ifdef __cplusplus
}
#endif

#endif /* GRAVITY_RING_H */
