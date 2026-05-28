#!/usr/bin/env bash
sudo modprobe nbd max_part=8
sudo qemu-nbd -d /dev/nbd0 2>/dev/null || true
sudo qemu-nbd -c /dev/nbd0 /mnt/OneClick-macOS-Simple-KVM/OpenCore.qcow2
sleep 1
sudo mkdir -p /mnt/oc_efi
sudo mount /dev/nbd0p2 /mnt/oc_efi || sudo mount /dev/nbd0p1 /mnt/oc_efi
sudo python3 -c "
import plistlib
with open('/mnt/oc_efi/EFI/OC/config.plist', 'rb') as f:
    pl = plistlib.load(f)
csr = pl['NVRAM']['Add']['7C436110-AB2A-4BBB-A880-FE41995C9F82'].get('csr-active-config')
print('csr-active-config:', csr.hex() if csr else 'Not set')
"
sudo umount /mnt/oc_efi
sudo qemu-nbd -d /dev/nbd0
