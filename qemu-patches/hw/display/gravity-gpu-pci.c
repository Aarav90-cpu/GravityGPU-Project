/*
 * gravity-gpu-pci.c — GravityGPU PCI wrapper
 *
 * Copyright (c) 2026 GravityGPU Project.
 */

#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#include "gravity-gpu.h"

static void G_GNUC_UNUSED gravity_gpu_pci_realize(PCIDevice *pci_dev, Error **errp)
{
    /* The real setup happens in gravity-gpu.c, this is just a stub if a separate
     * PCI wrapper class is needed for QEMU QOM structure.
     * In our current implementation, gravity-gpu.c implements the PCI device directly.
     */
}
