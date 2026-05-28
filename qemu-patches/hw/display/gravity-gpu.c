/*
 * gravity-gpu.c — GravityGPU Virtual PCI Device for QEMU
 *
 * Implements a custom PCI device that provides a shared memory region
 * and doorbell mechanism for Metal command transport between a macOS
 * guest and a Linux host translation daemon.
 *
 * Usage in QEMU command line:
 *   -device gravity-gpu,hostmem=512
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#define BUILDING_QEMU

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "sysemu/hostmem.h"
#include "qapi/error.h"
#include "qom/object.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "gravity-gpu.h"
#include "../../protocol/include/gravity_ring.h"
#include "../../protocol/include/gravity_protocol.h"

#define GRAVITY_GPU_DEFAULT_SHMEM_PATH "/gravity-gpu"

/* ═══════════════════════════════════════════════════════════════════════
 * BAR0 MMIO — Control Register Read/Write
 * ═══════════════════════════════════════════════════════════════════════ */

static uint64_t gravity_gpu_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    GravityGPUState *s = GRAVITY_GPU(opaque);

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gravity-gpu: bad MMIO read size %u at 0x%lx\n",
                      size, (unsigned long)addr);
        return 0;
    }

    switch (addr) {
    case GRAVITY_REG_MAGIC:
        return GRAVITY_PROTOCOL_MAGIC;

    case GRAVITY_REG_VERSION:
        return (GRAVITY_PROTOCOL_VERSION_MAJOR << 16) |
               GRAVITY_PROTOCOL_VERSION_MINOR;

    case GRAVITY_REG_DEVICE_STATUS:
        return s->device_status;

    case GRAVITY_REG_FEATURES:
        return s->device_features;

    case GRAVITY_REG_SHMEM_SIZE_LO:
        return (uint32_t)(s->shmem_size & 0xFFFFFFFF);

    case GRAVITY_REG_SHMEM_SIZE_HI:
        return (uint32_t)(s->shmem_size >> 32);

    case GRAVITY_REG_IRQ_STATUS:
        return s->irq_status;

    case GRAVITY_REG_IRQ_MASK:
        return s->irq_mask;

    case GRAVITY_REG_DISPLAY_WIDTH:
        return s->display_width;

    case GRAVITY_REG_DISPLAY_HEIGHT:
        return s->display_height;

    case GRAVITY_REG_DISPLAY_FORMAT:
        return s->display_format;

    case GRAVITY_REG_DISPLAY_STRIDE:
        /* Calculate stride: width * 4 (BGRA8), aligned to 256 bytes */
        return ((s->display_width * 4) + 255) & ~255u;

    case GRAVITY_REG_DEBUG:
        return s->debug_reg;

    case GRAVITY_REG_CMD_COUNT:
        return (uint32_t)s->cmd_count;

    case GRAVITY_REG_ERROR_CODE:
        return s->last_error;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gravity-gpu: read from unknown register 0x%lx\n",
                      (unsigned long)addr);
        return 0;
    }
}

static void gravity_gpu_mmio_write(void *opaque, hwaddr addr,
                                    uint64_t val, unsigned size)
{
    GravityGPUState *s = GRAVITY_GPU(opaque);

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gravity-gpu: bad MMIO write size %u at 0x%lx\n",
                      size, (unsigned long)addr);
        return;
    }

    switch (addr) {
    case GRAVITY_REG_DOORBELL:
        /*
         * Guest is notifying us that new commands have been written
         * to the command ring buffer in shared memory.
         *
         * In a production implementation, this would trigger an ioeventfd
         * that wakes up the host daemon's command processing thread.
         *
         * For now, we increment the command count and log it.
         */
        s->cmd_count++;
        qemu_log_mask(LOG_UNIMP,
                      "gravity-gpu: doorbell ring #%lu\n",
                      (unsigned long)s->cmd_count);

        /* TODO: Signal host daemon via eventfd/socket */
        /* event_notifier_set(&s->doorbell_notifier); */
        break;

    case GRAVITY_REG_IRQ_STATUS:
        /* Write 1 to clear interrupt bits */
        s->irq_status &= ~(uint32_t)val;
        /* De-assert interrupt if all cleared */
        if (s->irq_status == 0) {
            pci_set_irq(&s->parent_obj, 0);
        }
        break;

    case GRAVITY_REG_IRQ_MASK:
        s->irq_mask = (uint32_t)val;
        break;

    case GRAVITY_REG_GUEST_FEATURES:
        s->guest_features = (uint32_t)val;
        qemu_log_mask(LOG_UNIMP,
                      "gravity-gpu: guest features = 0x%08x\n",
                      s->guest_features);
        break;

    case GRAVITY_REG_GUEST_STATUS:
        s->guest_status = (uint32_t)val;
        qemu_log_mask(LOG_UNIMP,
                      "gravity-gpu: guest status = 0x%02x\n",
                      s->guest_status);

        if (s->guest_status == GRAVITY_GUEST_INIT) {
            qemu_log_mask(LOG_UNIMP,
                          "gravity-gpu: guest driver loaded!\n");
        } else if (s->guest_status == GRAVITY_GUEST_READY) {
            qemu_log_mask(LOG_UNIMP,
                          "gravity-gpu: guest driver ready, negotiation complete.\n");
        }
        break;

    case GRAVITY_REG_DISPLAY_WIDTH:
        s->display_width = (uint32_t)val;
        break;

    case GRAVITY_REG_DISPLAY_HEIGHT:
        s->display_height = (uint32_t)val;
        break;

    case GRAVITY_REG_DISPLAY_FORMAT:
        s->display_format = (uint32_t)val;
        break;

    case GRAVITY_REG_DEBUG:
        s->debug_reg = (uint32_t)val;
        qemu_log_mask(LOG_UNIMP,
                      "gravity-gpu: debug write = 0x%08x\n",
                      s->debug_reg);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gravity-gpu: write to unknown register 0x%lx = 0x%lx\n",
                      (unsigned long)addr, (unsigned long)val);
        break;
    }
}

static const MemoryRegionOps gravity_gpu_mmio_ops = {
    .read = gravity_gpu_mmio_read,
    .write = gravity_gpu_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* ═══════════════════════════════════════════════════════════════════════
 * BAR2 Shared Memory
 * ═══════════════════════════════════════════════════════════════════════ */

static uint64_t gravity_gpu_shmem_read(void *opaque, hwaddr addr, unsigned size)
{
    GravityGPUState *s = GRAVITY_GPU(opaque);

    if (addr + size > s->shmem_size) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gravity-gpu: shmem read out of bounds at 0x%lx\n",
                      (unsigned long)addr);
        return 0;
    }

    uint8_t *base = (uint8_t *)s->shmem_ptr + addr;

    switch (size) {
    case 1: return *(uint8_t *)base;
    case 2: return *(uint16_t *)base;
    case 4: return *(uint32_t *)base;
    case 8: return *(uint64_t *)base;
    default: return 0;
    }
}

static void gravity_gpu_shmem_write(void *opaque, hwaddr addr,
                                     uint64_t val, unsigned size)
{
    GravityGPUState *s = GRAVITY_GPU(opaque);

    if (addr + size > s->shmem_size) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gravity-gpu: shmem write out of bounds at 0x%lx\n",
                      (unsigned long)addr);
        return;
    }

    uint8_t *base = (uint8_t *)s->shmem_ptr + addr;

    switch (size) {
    case 1: *(uint8_t *)base  = (uint8_t)val;  break;
    case 2: *(uint16_t *)base = (uint16_t)val;  break;
    case 4: *(uint32_t *)base = (uint32_t)val;  break;
    case 8: *(uint64_t *)base = (uint64_t)val;  break;
    }
}

static const MemoryRegionOps gravity_gpu_shmem_ops = {
    .read = gravity_gpu_shmem_read,
    .write = gravity_gpu_shmem_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};


/* ═══════════════════════════════════════════════════════════════════════
 * Interrupt Helpers
 * ═══════════════════════════════════════════════════════════════════════ */

static void G_GNUC_UNUSED gravity_gpu_raise_irq(GravityGPUState *s, uint32_t irq_bits)
{
    s->irq_status |= irq_bits;

    if (s->irq_status & s->irq_mask) {
        /* TODO: Use MSI-X when available */
        pci_set_irq(&s->parent_obj, 1);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Device Lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

static void gravity_gpu_realize(PCIDevice *pci_dev, Error **errp)
{
    GravityGPUState *s = GRAVITY_GPU(pci_dev);
    const char *shm_name;

    /* Calculate shared memory size from property */
    if (s->hostmem_mb == 0) {
        s->hostmem_mb = GRAVITY_GPU_DEFAULT_SHMEM_SIZE / (1024 * 1024);
    }
    s->shmem_size = (uint64_t)s->hostmem_mb * 1024 * 1024;

    qemu_log_mask(LOG_UNIMP,
                  "gravity-gpu: initializing with %u MB shared memory\n",
                  s->hostmem_mb);

    /* Open POSIX shared memory so both QEMU and gravityd see the same region */
    shm_name = s->shmem_path ? s->shmem_path : GRAVITY_GPU_DEFAULT_SHMEM_PATH;
    s->shmem_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (s->shmem_fd < 0) {
        error_setg(errp, "gravity-gpu: shm_open('%s') failed: %s",
                   shm_name, strerror(errno));
        return;
    }
    
    /* Ensure world-rw regardless of umask */
    fchmod(s->shmem_fd, 0666);

    if (ftruncate(s->shmem_fd, s->shmem_size) < 0) {
        error_setg(errp, "gravity-gpu: ftruncate failed: %s", strerror(errno));
        close(s->shmem_fd);
        s->shmem_fd = -1;
        return;
    }

    s->shmem_ptr = mmap(NULL, s->shmem_size, PROT_READ | PROT_WRITE,
                        MAP_SHARED, s->shmem_fd, 0);
    if (s->shmem_ptr == MAP_FAILED) {
        error_setg(errp, "gravity-gpu: mmap failed: %s", strerror(errno));
        close(s->shmem_fd);
        s->shmem_fd = -1;
        s->shmem_ptr = NULL;
        return;
    }

    qemu_log_mask(LOG_UNIMP,
                  "gravity-gpu: POSIX shm '%s' mapped at %p (%u MB)\n",
                  shm_name, s->shmem_ptr, s->hostmem_mb);

    /* Initialize ring buffer in shared memory */
    if (gravity_ring_init_host(s->shmem_ptr, (uint32_t)s->shmem_size,
                                GRAVITY_DEFAULT_CMD_RING_SIZE,
                                GRAVITY_DEFAULT_COMP_RING_SIZE) < 0) {
        error_setg(errp, "gravity-gpu: failed to initialize ring buffer");
        munmap(s->shmem_ptr, s->shmem_size);
        close(s->shmem_fd);
        s->shmem_ptr = NULL;
        s->shmem_fd = -1;
        return;
    }

    /* Set up BAR0: MMIO control registers */
    memory_region_init_io(&s->mmio_bar, OBJECT(s), &gravity_gpu_mmio_ops, s,
                          "gravity-gpu-mmio", GRAVITY_GPU_BAR0_SIZE);
    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio_bar);

    /*
     * Set up BAR2: Shared memory region backed by POSIX shm.
     *
     * We MUST use memory_region_init_io() here so the BAR is mapped as MMIO
     * rather than system RAM. If we use memory_region_init_ram_ptr() or
     * memory_region_init_ram_device_ptr(), QEMU/UEFI treats it as RAM and adds
     * it to the memory map, which severely fragments the usable physical
     * address space during early boot and causes macOS to hang at EXITBS:START.
     *
     * The shmem ops will correctly read/write directly into s->shmem_ptr
     * (which is backed by the POSIX shm file).
     */
    memory_region_init_io(&s->shmem_bar, OBJECT(s), &gravity_gpu_shmem_ops, s,
                          "gravity-gpu-shmem", s->shmem_size);
    pci_register_bar(pci_dev, 2,
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64 |
                     PCI_BASE_ADDRESS_MEM_PREFETCH,
                     &s->shmem_bar);

    /* Initialize device state */
    s->device_features = GRAVITY_FEAT_RING_BUFFER |
                         GRAVITY_FEAT_SHADER_MSL |
                         GRAVITY_FEAT_COMPUTE |
                         GRAVITY_FEAT_BLIT |
                         GRAVITY_FEAT_DISPLAY;
    s->device_status = GRAVITY_STATUS_READY;
    s->guest_status = 0;
    s->guest_features = 0;
    s->irq_status = 0;
    s->irq_mask = GRAVITY_IRQ_CMD_COMPLETE | GRAVITY_IRQ_ERROR;

    /* Default display configuration */
    s->display_width = 1920;
    s->display_height = 1080;
    s->display_format = 80; /* BGRA8Unorm (MTLPixelFormatBGRA8Unorm) */

    s->cmd_count = 0;
    s->last_error = 0;
    s->debug_reg = 0;
    s->host_socket_fd = -1;

    qemu_log_mask(LOG_UNIMP,
                  "gravity-gpu: device realized successfully\n"
                  "  BAR0: MMIO 4KB control registers\n"
                  "  BAR2: %u MB shared memory (POSIX shm, ring + data)\n"
                  "  Features: 0x%08x\n",
                  s->hostmem_mb, s->device_features);
}

static void gravity_gpu_exit(PCIDevice *pci_dev)
{
    GravityGPUState *s = GRAVITY_GPU(pci_dev);

    qemu_log_mask(LOG_UNIMP, "gravity-gpu: device exiting\n");

    if (s->host_socket_fd >= 0) {
        /* close(s->host_socket_fd); */
        s->host_socket_fd = -1;
    }

    if (s->shmem_ptr && s->shmem_ptr != MAP_FAILED) {
        munmap(s->shmem_ptr, s->shmem_size);
        s->shmem_ptr = NULL;
    }

    if (s->shmem_fd >= 0) {
        close(s->shmem_fd);
        s->shmem_fd = -1;
        /* Don't shm_unlink — the daemon may still be using it */
    }
}

static void gravity_gpu_reset(DeviceState *dev)
{
    GravityGPUState *s = GRAVITY_GPU(dev);

    s->device_status = GRAVITY_STATUS_READY;
    s->guest_status = 0;
    s->guest_features = 0;
    s->irq_status = 0;
    s->cmd_count = 0;
    s->last_error = 0;
    s->debug_reg = 0;

    /* Re-initialize ring buffer */
    if (s->shmem_ptr) {
        gravity_ring_init_host(s->shmem_ptr, (uint32_t)s->shmem_size,
                                GRAVITY_DEFAULT_CMD_RING_SIZE,
                                GRAVITY_DEFAULT_COMP_RING_SIZE);
    }

    qemu_log_mask(LOG_UNIMP, "gravity-gpu: device reset\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * VM State (Migration/Save-Restore)
 * ═══════════════════════════════════════════════════════════════════════ */

static const VMStateDescription vmstate_gravity_gpu = {
    .name = "gravity-gpu",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj, GravityGPUState),
        VMSTATE_UINT32(device_status, GravityGPUState),
        VMSTATE_UINT32(guest_features, GravityGPUState),
        VMSTATE_UINT32(guest_status, GravityGPUState),
        VMSTATE_UINT32(irq_status, GravityGPUState),
        VMSTATE_UINT32(irq_mask, GravityGPUState),
        VMSTATE_UINT32(display_width, GravityGPUState),
        VMSTATE_UINT32(display_height, GravityGPUState),
        VMSTATE_UINT32(display_format, GravityGPUState),
        VMSTATE_UINT32(debug_reg, GravityGPUState),
        VMSTATE_UINT32(last_error, GravityGPUState),
        VMSTATE_END_OF_LIST()
    },
};

/* ═══════════════════════════════════════════════════════════════════════
 * Properties
 * ═══════════════════════════════════════════════════════════════════════ */

static Property gravity_gpu_properties[] = {
    DEFINE_PROP_UINT32("hostmem", GravityGPUState, hostmem_mb,
                       GRAVITY_GPU_DEFAULT_SHMEM_SIZE / (1024 * 1024)),
    DEFINE_PROP_STRING("socket-path", GravityGPUState, host_socket_path),
    DEFINE_PROP_STRING("shmem-path", GravityGPUState, shmem_path),
    DEFINE_PROP_END_OF_LIST(),
};

/* ═══════════════════════════════════════════════════════════════════════
 * QOM Type Registration
 * ═══════════════════════════════════════════════════════════════════════ */

static void gravity_gpu_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->realize     = gravity_gpu_realize;
    pc->exit        = gravity_gpu_exit;
    pc->vendor_id   = GRAVITY_GPU_PCI_VENDOR_ID;
    pc->device_id   = GRAVITY_GPU_PCI_DEVICE_ID;
    pc->revision    = GRAVITY_GPU_PCI_REVISION;
    pc->class_id    = GRAVITY_GPU_PCI_CLASS;
    pc->subsystem_vendor_id = GRAVITY_GPU_PCI_SUBSYSTEM_VENDOR;
    pc->subsystem_id = GRAVITY_GPU_PCI_SUBSYSTEM_ID;

    dc->desc        = "GravityGPU - Metal to Vulkan Paravirtual GPU";
    dc->reset       = gravity_gpu_reset;
    dc->vmsd        = &vmstate_gravity_gpu;
    device_class_set_props(dc, gravity_gpu_properties);

    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static void gravity_gpu_instance_init(Object *obj)
{
    /* Instance initialization if needed */
}

static const TypeInfo gravity_gpu_info = {
    .name           = TYPE_GRAVITY_GPU,
    .parent         = TYPE_PCI_DEVICE,
    .instance_size  = sizeof(GravityGPUState),
    .instance_init  = gravity_gpu_instance_init,
    .class_init     = gravity_gpu_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void gravity_gpu_register_types(void)
{
    type_register_static(&gravity_gpu_info);
}

type_init(gravity_gpu_register_types)
