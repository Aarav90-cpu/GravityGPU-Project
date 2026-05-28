#!/bin/bash
# scripts/launch-vm.sh
# Launches macOS VM with GravityGPU attached.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE_DIR="$(realpath "$SCRIPT_DIR/..")"
GRAVITYD="$WORKSPACE_DIR/build/gravityd"
BASIC_SH="/mnt/OneClick-macOS-Simple-KVM/basic.sh"

# Fix boot order issue by injecting bootindex=1 to OpenCore if missing
if ! grep -q bootindex "$BASIC_SH"; then
    echo "Patching basic.sh to fix boot order..."
    sudo sed -i 's/-device ide-hd,bus=sata.2,drive=OpenCore \\/-device ide-hd,bus=sata.2,drive=OpenCore,bootindex=1 \\/' "$BASIC_SH"
fi

# Add SSH port forwarding for kext deployment
if ! grep -q "hostfwd" "$BASIC_SH"; then
    echo "Patching basic.sh to add SSH port forwarding (host:2222 -> guest:22)..."
    sudo sed -i 's/-netdev user,id=net0/-netdev user,id=net0,hostfwd=tcp::2222-:22/' "$BASIC_SH"
fi

# Rebuild gravityd if needed
if [ ! -x "$GRAVITYD" ]; then
    echo "Building gravityd..."
    cd "$WORKSPACE_DIR/build" && cmake .. && make gravityd -j$(nproc)
fi

echo "Starting GravityGPU Daemon..."
"$GRAVITYD" -m 32 &
DAEMON_PID=$!

# Give daemon time to initialize shared memory before QEMU starts
sleep 1

echo "Launching macOS VM..."
sudo "$BASIC_SH"

echo "VM exited."

kill $DAEMON_PID 2>/dev/null || true
