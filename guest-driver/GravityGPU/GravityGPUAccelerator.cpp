/*
 * GravityGPUAccelerator.cpp — macOS Virtual GPU Driver Implementation
 *
 * IOService driver that matches the GravityGPU PCI device,
 * maps the MMIO + shared memory BARs, initializes the ring buffer,
 * and performs protocol handshake with the host daemon.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include "GravityGPUAccelerator.hpp"

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Logging
 * ═══════════════════════════════════════════════════════════════════════ */

#define GRAVITY_LOG(fmt, ...) \
    IOLog("GravityGPU: " fmt "\n", ##__VA_ARGS__)

#define GRAVITY_ERR(fmt, ...) \
    IOLog("GravityGPU ERROR: " fmt "\n", ##__VA_ARGS__)

/* ═══════════════════════════════════════════════════════════════════════
 * OSDeclare
 * ═══════════════════════════════════════════════════════════════════════ */

OSDefineMetaClassAndStructors(GravityGPUAccelerator, IOService)

/* ═══════════════════════════════════════════════════════════════════════
 * IOService Lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

bool GravityGPUAccelerator::init(OSDictionary* dict)
{
    if (!IOService::init(dict)) {
        GRAVITY_ERR("IOService::init failed");
        return false;
    }

    fPCIDevice = nullptr;
    fBAR0Map = nullptr;
    fBAR2Map = nullptr;
    fMMIO = nullptr;
    fSharedMemory = nullptr;
    fSharedMemorySize = 0;
    fSequence = 0;

    memset(&fRing, 0, sizeof(fRing));

    GRAVITY_LOG("init");
    return true;
}

void GravityGPUAccelerator::free()
{
    GRAVITY_LOG("free");
    unmapBARs();
    IOService::free();
}

IOService* GravityGPUAccelerator::probe(IOService* provider, SInt32* score)
{
    GRAVITY_LOG("probe — checking PCI device");

    IOPCIDevice* pciDev = OSDynamicCast(IOPCIDevice, provider);
    if (!pciDev) {
        GRAVITY_ERR("Provider is not a PCI device");
        return nullptr;
    }

    /* Verify vendor/device ID */
    uint16_t vendorID = pciDev->configRead16(kIOPCIConfigVendorID);
    uint16_t deviceID = pciDev->configRead16(kIOPCIConfigDeviceID);

    GRAVITY_LOG("probe — PCI vendor=0x%04x device=0x%04x", vendorID, deviceID);

    if (vendorID != 0x1AF4 || deviceID != 0x10DE) {
        GRAVITY_ERR("PCI ID mismatch — not our device");
        return nullptr;
    }

    /* Boost our probe score to ensure we match */
    if (score) {
        *score = 10000;
    }

    GRAVITY_LOG("probe — matched GravityGPU device!");
    return this;
}

bool GravityGPUAccelerator::start(IOService* provider)
{
    GRAVITY_LOG("═══════════════════════════════════════════");
    GRAVITY_LOG("  GravityGPU Virtual GPU Driver");
    GRAVITY_LOG("  Metal → Vulkan Paravirtualization");
    GRAVITY_LOG("═══════════════════════════════════════════");

    if (!IOService::start(provider)) {
        GRAVITY_ERR("IOService::start failed");
        return false;
    }

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice) {
        GRAVITY_ERR("Cannot get PCI device");
        return false;
    }

    /* Enable PCI bus mastering and memory space access */
    fPCIDevice->setBusMasterEnable(true);
    fPCIDevice->setMemoryEnable(true);

    /* Map BARs */
    if (!mapBARs()) {
        GRAVITY_ERR("Failed to map PCI BARs");
        return false;
    }

    /* Initialize device */
    if (!initializeDevice()) {
        GRAVITY_ERR("Failed to initialize device");
        unmapBARs();
        return false;
    }

    /* Negotiate features with host */
    if (!negotiateFeatures()) {
        GRAVITY_ERR("Feature negotiation failed");
        unmapBARs();
        return false;
    }

    /* Register with IOService */
    registerService();

    GRAVITY_LOG("start — device initialized successfully!");
    GRAVITY_LOG("  Shared memory: %llu MB", fSharedMemorySize / (1024*1024));
    GRAVITY_LOG("  Ring buffer: %u KB cmd, %u KB comp",
                fRing.cmd_ring_size / 1024, fRing.comp_ring_size / 1024);
    GRAVITY_LOG("  Data region: %u MB",
                fRing.data_region_size / (1024*1024));

    return true;
}

void GravityGPUAccelerator::stop(IOService* provider)
{
    GRAVITY_LOG("stop — shutting down");

    /* Send goodbye to host */
    gravity_cmd_header_t goodbye;
    gravity_cmd_init(&goodbye, GRAVITY_CMD_GOODBYE, sizeof(goodbye), nextSequence());
    submitCommand(&goodbye, sizeof(goodbye));

    /* Notify host we're leaving */
    mmioWrite32(GRAVITY_REG_GUEST_STATUS, GRAVITY_GUEST_ERROR);

    unmapBARs();
    IOService::stop(provider);
}

/* ═══════════════════════════════════════════════════════════════════════
 * BAR Mapping
 * ═══════════════════════════════════════════════════════════════════════ */

bool GravityGPUAccelerator::mapBARs()
{
    /* Map BAR0 (MMIO Control Registers) */
    fBAR0Map = fPCIDevice->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (!fBAR0Map) {
        GRAVITY_ERR("Cannot map BAR0 (MMIO)");
        return false;
    }

    fMMIO = (volatile uint32_t*)fBAR0Map->getVirtualAddress();
    GRAVITY_LOG("BAR0 mapped at %p, size=%llu",
                fMMIO, fBAR0Map->getLength());

    /* Verify magic value */
    uint32_t magic = mmioRead32(GRAVITY_REG_MAGIC);
    if (magic != GRAVITY_PROTOCOL_MAGIC) {
        GRAVITY_ERR("BAR0 magic mismatch: got 0x%08x, expected 0x%08x",
                    magic, GRAVITY_PROTOCOL_MAGIC);
        return false;
    }

    GRAVITY_LOG("BAR0 magic OK: 0x%08x (GRVY)", magic);

    /* Map BAR2 (Shared Memory) */
    fBAR2Map = fPCIDevice->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress2);
    if (!fBAR2Map) {
        GRAVITY_ERR("Cannot map BAR2 (shared memory)");
        return false;
    }

    fSharedMemory = (void*)fBAR2Map->getVirtualAddress();
    fSharedMemorySize = fBAR2Map->getLength();
    GRAVITY_LOG("BAR2 mapped at %p, size=%llu MB",
                fSharedMemory, fSharedMemorySize / (1024*1024));

    return true;
}

void GravityGPUAccelerator::unmapBARs()
{
    if (fBAR2Map) {
        fBAR2Map->release();
        fBAR2Map = nullptr;
    }
    if (fBAR0Map) {
        fBAR0Map->release();
        fBAR0Map = nullptr;
    }
    fMMIO = nullptr;
    fSharedMemory = nullptr;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Device Initialization
 * ═══════════════════════════════════════════════════════════════════════ */

bool GravityGPUAccelerator::initializeDevice()
{
    /* Read protocol version from device */
    uint32_t version = mmioRead32(GRAVITY_REG_VERSION);
    uint16_t major = (version >> 16) & 0xFFFF;
    uint16_t minor = version & 0xFFFF;
    GRAVITY_LOG("Host protocol v%u.%u", major, minor);

    /* Check device status */
    uint32_t status = mmioRead32(GRAVITY_REG_DEVICE_STATUS);
    if (!(status & GRAVITY_STATUS_READY)) {
        GRAVITY_ERR("Device not ready (status=0x%08x)", status);
        return false;
    }

    /* Attach to ring buffer in shared memory */
    if (gravity_ring_attach(&fRing, fSharedMemory) < 0) {
        GRAVITY_ERR("Failed to attach to ring buffer");
        return false;
    }

    GRAVITY_LOG("Ring buffer attached successfully");

    /* Tell host we're initialized */
    mmioWrite32(GRAVITY_REG_GUEST_STATUS, GRAVITY_GUEST_INIT);

    return true;
}

bool GravityGPUAccelerator::negotiateFeatures()
{
    /* Read host features */
    uint32_t host_features = mmioRead32(GRAVITY_REG_FEATURES);
    GRAVITY_LOG("Host features: 0x%08x", host_features);

    /* Write our supported features (acknowledge what we support) */
    uint32_t our_features = GRAVITY_FEAT_RING_BUFFER |
                            GRAVITY_FEAT_SHADER_MSL |
                            GRAVITY_FEAT_DISPLAY;
    mmioWrite32(GRAVITY_REG_GUEST_FEATURES, our_features);
    GRAVITY_LOG("Guest features: 0x%08x", our_features);

    /* Send HELLO command */
    gravity_cmd_hello_t hello;
    gravity_cmd_init(&hello.hdr, GRAVITY_CMD_HELLO, sizeof(hello), nextSequence());
    hello.magic = GRAVITY_PROTOCOL_MAGIC;
    hello.version_major = GRAVITY_PROTOCOL_VERSION_MAJOR;
    hello.version_minor = GRAVITY_PROTOCOL_VERSION_MINOR;
    hello.guest_features = our_features;
    memset(hello.reserved, 0, sizeof(hello.reserved));

    if (submitCommand(&hello, sizeof(hello)) < 0) {
        GRAVITY_ERR("Failed to send HELLO");
        return false;
    }

    /* Ring the doorbell to wake up host */
    mmioWrite32(GRAVITY_REG_DOORBELL, 1);

    /* Wait for response (poll with timeout) */
    uint8_t resp_buf[256];
    int max_polls = 1000;
    int resp_size = 0;

    while (max_polls-- > 0) {
        resp_size = readCompletion(resp_buf, sizeof(resp_buf));
        if (resp_size > 0) break;
        IODelay(1000);  /* 1ms delay */
    }

    if (resp_size <= 0) {
        GRAVITY_LOG("No HELLO response from host (host daemon may not be running yet)");
        /* This is OK — the host daemon may start later */
    } else {
        gravity_cmd_hello_response_t* resp = (gravity_cmd_hello_response_t*)resp_buf;
        GRAVITY_LOG("Host responded: v%u.%u, max_texture=%u, max_buffer=%u",
                    resp->version_major, resp->version_minor,
                    resp->max_texture_size, resp->max_buffer_size);
    }

    /* Mark guest as ready */
    mmioWrite32(GRAVITY_REG_GUEST_STATUS, GRAVITY_GUEST_READY);
    GRAVITY_LOG("Feature negotiation complete — guest READY");

    return true;
}

/* ═══════════════════════════════════════════════════════════════════════
 * MMIO Access
 * ═══════════════════════════════════════════════════════════════════════ */

uint32_t GravityGPUAccelerator::mmioRead32(uint32_t offset)
{
    if (!fMMIO) return 0;
    return fMMIO[offset / 4];
}

void GravityGPUAccelerator::mmioWrite32(uint32_t offset, uint32_t value)
{
    if (!fMMIO) return;
    fMMIO[offset / 4] = value;
    __asm__ volatile("mfence" ::: "memory");
}

/* ═══════════════════════════════════════════════════════════════════════
 * Ring Buffer Interface
 * ═══════════════════════════════════════════════════════════════════════ */

int GravityGPUAccelerator::submitCommand(const void* cmd, uint32_t size)
{
    int result = gravity_ring_cmd_write(&fRing, cmd, size);
    if (result == 0) {
        /* Ring doorbell to notify host */
        mmioWrite32(GRAVITY_REG_DOORBELL, 1);
    }
    return result;
}

int GravityGPUAccelerator::readCompletion(void* out_buf, uint32_t buf_size)
{
    return gravity_ring_comp_read(&fRing, out_buf, buf_size);
}

int GravityGPUAccelerator::writeData(const void* data, uint32_t size,
                                       uint64_t* out_offset)
{
    return gravity_ring_data_write(&fRing, data, size, out_offset);
}

uint32_t GravityGPUAccelerator::nextSequence()
{
    return ++fSequence;
}
