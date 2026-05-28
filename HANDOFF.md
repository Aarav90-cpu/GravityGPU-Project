# GravityGPU Project Handoff

Welcome to the GravityGPU project! This document outlines the current state of the architecture, what has been accomplished so far, and the immediate next steps to continue development.

## 🏗️ Architecture Overview

GravityGPU is a paravirtualized graphics driver designed to expose host graphics computation to a macOS QEMU virtual machine. The architecture consists of four main components:

1. **Host Daemon (`gravityd`)**: A background process running on the host Linux machine that processes commands from the guest VM via a shared memory ring buffer.
2. **QEMU PCI Device (`gravity-gpu`)**: A custom QEMU device that exposes two PCI BARs to the guest:
   - `BAR0`: MMIO control registers
   - `BAR2`: Shared memory ring buffer
3. **Guest Kernel Extension (`GravityGPU.kext`)**: A macOS IOKit driver (`IOService` and `IOUserClient`) that binds to the PCI device and securely exposes the shared memory ring to macOS userspace applications.
4. **Guest Userspace (`GravityGPU_TestApp`)**: A C++ test application that connects to the kernel extension using `IOServiceOpen` and submits raw compute commands.

## 🚀 Current Progress

- **QEMU & Host Daemon**: The shared memory interface (`gravity_ring.h`) is fully functional. `launch-vm.sh` successfully boots the VM and daemon.
- **Guest Kext**: `GravityGPUAccelerator.cpp` successfully binds to the `0x10DE1AF4` PCI device. The `GravityGPUUserClient` has been implemented to allow userspace connections.
- **Deployment**: We have established a robust deployment pipeline. Since macOS Big Sur strictly enforces AMFI and prevents dynamic loading of unsigned kexts, we use OpenCore injection.

## ⚠️ Current Blocker

The latest iteration of `GravityGPU.kext` (which introduced `GravityGPUUserClient`) is currently failing to load on boot when injected via OpenCore. 

**Immediate next step**: Run the `debug-kext.sh` script inside the VM to diagnose why the kext is being rejected. It is highly likely a missing symbol or linking error in the `GravityGPUUserClient` implementation.
```bash
# On the host, run this to ask the VM why the kext failed to load:
./scripts/debug-kext.sh
```

## 🛠️ Helpful Scripts

All scripts are located in the `scripts/` directory:

- `launch-vm.sh`: Starts the host daemon and the QEMU macOS VM.
- `deploy-kext.sh`: Compiles the kext inside the VM (requires VM to be running).
- `fetch-kext.sh`: Pulls the successfully compiled kext from the VM back to the host.
- `inject-kext-opencore.sh`: Mounts `OpenCore.qcow2` and injects the kext so it loads on the next boot (requires VM to be powered off).
- `run-test-app.sh`: Compiles and runs the userspace test application inside the VM.
- `debug-kext.sh`: Runs `kextutil` on the VM to trace kext loading errors.

## 🗺️ Roadmap: The Metal Compute Accelerator

Once the `IOUserClient` connection is stable and the test application successfully sends a `GRAVITY_CMD_PING` to the host daemon, the next major phase is to build the **macOS Userspace Metal Driver (UMD)**. 

This will involve creating a `.bundle` that conforms to Apple's `MTLDevice` protocols, allowing macOS to offload heavy rendering tasks to GravityGPU while relying on QXL for basic display output.
