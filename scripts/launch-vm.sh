#!/bin/bash
# scripts/launch-vm.sh
# Launches macOS VM with GravityGPU attached.

set -e

echo "Starting GravityGPU Daemon..."
../build/gravityd &
DAEMON_PID=$!

echo "Launching macOS VM..."
# qemu-system-x86_64 \
#   -m 8G -cpu host -enable-kvm \
#   -device gravity-gpu,hostmem=512 \
#   -drive file=macos.qcow2,format=qcow2

echo "VM launch script stub."

kill $DAEMON_PID
