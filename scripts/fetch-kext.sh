#!/usr/bin/env bash
echo "[*] Fetching built GravityGPU.kext from VM..."
mkdir -p /home/aarav/MACOSGRAPHICS/guest-driver/build
scp -o StrictHostKeyChecking=no -P 2222 -r aarav@localhost:/tmp/gravitygpu/guest-driver/build/GravityGPU.kext /home/aarav/MACOSGRAPHICS/guest-driver/build/
echo "[*] Done! Kext saved to guest-driver/build/GravityGPU.kext"
