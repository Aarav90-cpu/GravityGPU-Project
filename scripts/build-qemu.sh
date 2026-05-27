#!/bin/bash
# scripts/build-qemu.sh
# Applies patches and builds QEMU with GravityGPU support.

set -e

WORKSPACE_DIR=$(realpath "$(dirname "$0")/..")
QEMU_DIR="$WORKSPACE_DIR/qemu"
QEMU_VERSION="v8.2.0"

echo "========================================================"
echo " Building Custom QEMU ($QEMU_VERSION) with GravityGPU"
echo "========================================================"

if [ ! -d "$QEMU_DIR" ]; then
    echo "[*] Cloning QEMU ($QEMU_VERSION)..."
    git clone --depth 1 -b $QEMU_VERSION https://gitlab.com/qemu-project/qemu.git "$QEMU_DIR"
else
    echo "[*] QEMU source already exists. Skipping clone."
fi

echo "[*] Copying GravityGPU device files to QEMU..."
cp "$WORKSPACE_DIR/qemu-patches/hw/display/gravity-gpu.h" "$QEMU_DIR/hw/display/"
cp "$WORKSPACE_DIR/qemu-patches/hw/display/gravity-gpu.c" "$QEMU_DIR/hw/display/"
cp "$WORKSPACE_DIR/qemu-patches/hw/display/gravity-gpu-pci.c" "$QEMU_DIR/hw/display/"

echo "[*] Copying protocol headers to QEMU..."
cp -r "$WORKSPACE_DIR/protocol" "$QEMU_DIR/protocol"

echo "[*] Patching hw/display/meson.build..."
# Check if we already patched it
if ! grep -q "CONFIG_GRAVITY_GPU" "$QEMU_DIR/hw/display/meson.build"; then
    cat << 'EOF' >> "$QEMU_DIR/hw/display/meson.build"

# GravityGPU Custom Device
system_ss.add(when: 'CONFIG_GRAVITY_GPU', if_true: files(
  'gravity-gpu.c',
  'gravity-gpu-pci.c'
))
EOF
    echo " -> meson.build patched successfully."
else
    echo " -> meson.build already patched."
fi

echo "[*] Patching hw/display/Kconfig..."
if ! grep -q "config GRAVITY_GPU" "$QEMU_DIR/hw/display/Kconfig"; then
    cat << 'EOF' >> "$QEMU_DIR/hw/display/Kconfig"

config GRAVITY_GPU
    bool
    default y
    depends on PCI
EOF
    echo " -> Kconfig patched successfully."
else
    echo " -> Kconfig already patched."
fi

echo "[*] Configuring QEMU..."
cd "$QEMU_DIR"
./configure --target-list=x86_64-softmmu --enable-kvm

echo "[*] Building QEMU (this may take a while)..."
make -j$(nproc)

echo "========================================================"
echo " QEMU build complete!"
echo " Binary is located at: $QEMU_DIR/build/qemu-system-x86_64"
echo "========================================================"
