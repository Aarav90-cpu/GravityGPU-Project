/*
 * gravity-gpu.h — GravityGPU Virtual PCI Device Header
 *
 * Defines the QEMU device structures and PCI register layout for the
 * GravityGPU virtual GPU device. The macOS guest driver matches this
 * device by vendor/device ID and maps its BARs.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#ifndef HW_GRAVITY_GPU_H
#define HW_GRAVITY_GPU_H

/*
 * NOTE: This header is designed to compile both within the QEMU build
 * system and standalone (for testing). QEMU-specific includes are
 * guarded by BUILDING_QEMU.
 */

#ifdef BUILDING_QEMU
#include "hw/pci/pci_device.h"
#include "qemu/event_notifier.h"
#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qom/object.h"
#endif

#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════
 * PCI Identity
 * ═══════════════════════════════════════════════════════════════════════ */

#define GRAVITY_GPU_PCI_VENDOR_ID 0x1AF4 /* Red Hat / Virtio vendor space */
#define GRAVITY_GPU_PCI_DEVICE_ID                                              \
  0x10DE /* Custom device ID (0x10DE chosen                                    \
            to hint "GPU" — not conflicting                                  \
            with real NVIDIA since we use                                      \
            virtio vendor ID) */
#define GRAVITY_GPU_PCI_REVISION 0x01
#define GRAVITY_GPU_PCI_CLASS 0x030000 /* VGA compatible controller */
#define GRAVITY_GPU_PCI_SUBSYSTEM_VENDOR 0x1AF4
#define GRAVITY_GPU_PCI_SUBSYSTEM_ID 0x1100

/* ═══════════════════════════════════════════════════════════════════════
 * BAR Layout
 * ═══════════════════════════════════════════════════════════════════════
 *
 * BAR0 (MMIO, 4KB):  Control registers
 * BAR2 (MMIO, configurable): Shared memory region (ring buffer + data)
 */

#define GRAVITY_GPU_BAR0_SIZE 4096 /* 4 KB control registers */

/* Default shared memory size (512 MB) — overridable via QEMU property */
#define GRAVITY_GPU_DEFAULT_SHMEM_SIZE (512 * 1024 * 1024)

/* ═══════════════════════════════════════════════════════════════════════
 * BAR0 Register Map (MMIO Control Registers)
 * ═══════════════════════════════════════════════════════════════════════
 *
 * All registers are 32-bit, accessed at 4-byte aligned offsets.
 */

/* Device identification (read-only) */
#define GRAVITY_REG_MAGIC 0x000 /* R:  Always reads 0x47525659 ("GRVY") */
#define GRAVITY_REG_VERSION                                                    \
  0x004 /* R:  Protocol version (major << 16 | minor) */
#define GRAVITY_REG_DEVICE_STATUS 0x008 /* R:  Device status bits */
#define GRAVITY_REG_FEATURES 0x00C      /* R:  Supported feature bits */

/* Shared memory info (read-only) */
#define GRAVITY_REG_SHMEM_SIZE_LO 0x010 /* R:  Shared memory size (low 32) */
#define GRAVITY_REG_SHMEM_SIZE_HI 0x014 /* R:  Shared memory size (high 32) */

/* Guest → Host doorbell (write-only) */
#define GRAVITY_REG_DOORBELL                                                   \
  0x020 /* W:  Writing any value notifies host of new commands in ring buffer  \
         */

/* Interrupt control */
#define GRAVITY_REG_IRQ_STATUS                                                 \
  0x030 /* R/W: Interrupt status (write 1 to clear) */
#define GRAVITY_REG_IRQ_MASK 0x034 /* R/W: Interrupt mask (1 = enabled) */

/* Guest feature negotiation */
#define GRAVITY_REG_GUEST_FEATURES                                             \
  0x040 /* W:  Guest writes its feature bits here */
#define GRAVITY_REG_GUEST_STATUS                                               \
  0x044 /* W:  Guest status (INIT, READY, ERROR) */

/* Display configuration */
#define GRAVITY_REG_DISPLAY_WIDTH 0x050  /* R/W: Display width in pixels */
#define GRAVITY_REG_DISPLAY_HEIGHT 0x054 /* R/W: Display height in pixels */
#define GRAVITY_REG_DISPLAY_FORMAT 0x058 /* R/W: Display pixel format */
#define GRAVITY_REG_DISPLAY_STRIDE                                             \
  0x05C /* R:   Display stride (bytes per row) */

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
 * Ring Buffer Configuration Constants
 * ═══════════════════════════════════════════════════════════════════════ */

/* Default sizes within the shared memory region */
#define GRAVITY_DEFAULT_CMD_RING_SIZE (4 * 1024 * 1024) /* 4 MB command ring   \
                                                         */
#define GRAVITY_DEFAULT_COMP_RING_SIZE (256 * 1024) /* 256 KB completion ring  \
                                                     */
/* Remainder of shared memory is data region */

/* ═══════════════════════════════════════════════════════════════════════
 * QEMU Device State (only when building within QEMU)
 * ═══════════════════════════════════════════════════════════════════════ */

#ifdef BUILDING_QEMU

#define TYPE_GRAVITY_GPU "gravity-gpu"
OBJECT_DECLARE_SIMPLE_TYPE(GravityGPUState, GRAVITY_GPU)

typedef struct GravityGPUState {
  /*< private >*/
  PCIDevice parent_obj;

  /*< public >*/

  /* BAR0: MMIO control registers */
  MemoryRegion mmio_bar;

  /* BAR2: Shared memory region */
  MemoryRegion shmem_bar;
  void *shmem_ptr;     /* mmap'd shared memory */
  uint64_t shmem_size; /* Configured size */

  /* Device state */
  uint32_t device_status;
  uint32_t device_features;
  uint32_t guest_features;
  uint32_t guest_status;
  uint32_t irq_status;
  uint32_t irq_mask;

  /* Display state */
  uint32_t display_width;
  uint32_t display_height;
  uint32_t display_format;

  /* Debug/statistics */
  uint32_t debug_reg;
  uint64_t cmd_count;
  uint32_t last_error;

  /* Doorbell notification */
  EventNotifier doorbell_notifier; /* ioeventfd for doorbell MMIO writes */

  /* Host daemon communication */
  int host_socket_fd;     /* Unix socket to gravityd */
  char *host_socket_path; /* Path to Unix socket */

  /* Properties (set via QEMU command line) */
  uint32_t hostmem_mb; /* Shared memory size in MB */

} GravityGPUState;

#endif /* BUILDING_QEMU */

#endif /* HW_GRAVITY_GPU_H */
