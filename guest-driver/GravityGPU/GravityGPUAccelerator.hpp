/*
 * GravityGPUAccelerator.hpp — macOS Virtual GPU Driver Header
 *
 * IOAccelerator family driver that appears as a GPU device to macOS.
 * Intercepts Metal commands and serializes them to the ring buffer
 * in the QEMU PCI device's shared memory BAR.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#ifndef GRAVITYGPUACCELERATOR_HPP
#define GRAVITYGPUACCELERATOR_HPP

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

/* Protocol headers (shared with host) */
extern "C" {
#include "../../protocol/include/gravity_protocol.h"
#include "../../protocol/include/gravity_ring.h"
}

/* ═══════════════════════════════════════════════════════════════════════
 * GravityGPUAccelerator — IOService subclass for PCI device matching
 * ═══════════════════════════════════════════════════════════════════════
 *
 * This driver matches our custom PCI device (vendor 0x1AF4, device 0x10DE)
 * that QEMU creates, maps its BARs, and provides the IOAccelerator
 * interface that macOS WindowServer / Metal.framework expects.
 *
 * NOTE: In a production driver, this would subclass IOAccelerator or
 * IOGraphicsAccelerator2. Since those are private Apple classes, we
 * start with IOService and will progressively implement the IOUserClient
 * methods that Metal.framework calls.
 */

class GravityGPUAccelerator : public IOService
{
    OSDeclareDefaultStructors(GravityGPUAccelerator)

public:
    /* ── IOService Lifecycle ── */
    virtual bool        init(OSDictionary* dict = nullptr) override;
    virtual void        free() override;
    virtual bool        start(IOService* provider) override;
    virtual void        stop(IOService* provider) override;
    virtual IOService*  probe(IOService* provider, SInt32* score) override;

    /* ── Ring Buffer Interface ── */

    /**
     * Write a command to the ring buffer and ring the doorbell.
     * @return 0 on success, -1 if ring is full.
     */
    int submitCommand(const void* cmd, uint32_t size);

    /**
     * Read a completion from the completion ring.
     * @return Completion size in bytes, 0 if none available, -1 on error.
     */
    int readCompletion(void* out_buf, uint32_t buf_size);

    /**
     * Allocate space in the data region for bulk transfers.
     */
    int writeData(const void* data, uint32_t size, uint64_t* out_offset);

    /**
     * Get the current sequence number for command ordering.
     */
    uint32_t nextSequence();

protected:
    /* ── PCI Device Access ── */
    IOPCIDevice*        fPCIDevice;         /* Our PCI provider */

    /* BAR mappings */
    IOMemoryMap*        fBAR0Map;           /* MMIO control registers */
    IOMemoryMap*        fBAR2Map;           /* Shared memory region */
    volatile uint32_t*  fMMIO;              /* BAR0 virtual address */
    void*               fSharedMemory;      /* BAR2 virtual address */
    uint64_t            fSharedMemorySize;

    /* Ring buffer context */
    gravity_ring_t      fRing;

    /* Command sequence counter */
    uint32_t            fSequence;

    /* ── MMIO Register Access ── */
    uint32_t            mmioRead32(uint32_t offset);
    void                mmioWrite32(uint32_t offset, uint32_t value);

    /* ── Device Initialization ── */
    bool                mapBARs();
    void                unmapBARs();
    bool                initializeDevice();
    bool                negotiateFeatures();
};

#endif /* GRAVITYGPUACCELERATOR_HPP */
