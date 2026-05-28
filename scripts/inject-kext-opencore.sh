#!/usr/bin/env bash
set -e

if pgrep qemu-system-x86_64 > /dev/null; then
    echo "ERROR: QEMU is currently running! Please shut down the macOS VM first."
    exit 1
fi

KEXT_DIR="/home/aarav/MACOSGRAPHICS/guest-driver/build/GravityGPU.kext"
if [ ! -d "$KEXT_DIR" ]; then
    echo "ERROR: $KEXT_DIR not found. Build it first."
    exit 1
fi

OC_IMAGE="/mnt/OneClick-macOS-Simple-KVM/OpenCore.qcow2"
echo "[*] Loading NBD kernel module..."
sudo modprobe nbd max_part=8
sudo qemu-nbd -d /dev/nbd0 2>/dev/null || true
sudo qemu-nbd -c /dev/nbd0 "$OC_IMAGE"
sleep 1

echo "[*] Discovering EFI partition..."
sudo mkdir -p /mnt/oc_efi
PLIST=""
for part in /dev/nbd0p*; do
    sudo mount "$part" /mnt/oc_efi 2>/dev/null || continue
    if [ -f "/mnt/oc_efi/EFI/OC/config.plist" ]; then
        PLIST="/mnt/oc_efi/EFI/OC/config.plist"
        echo "[+] Found OpenCore config at $part"
        break
    fi
    sudo umount /mnt/oc_efi 2>/dev/null || true
done

if [ -z "$PLIST" ]; then
    echo "ERROR: config.plist not found!"
    sudo qemu-nbd -d /dev/nbd0
    exit 1
fi

echo "[*] Copying GravityGPU.kext to EFI/OC/Kexts..."
sudo rm -rf /mnt/oc_efi/EFI/OC/Kexts/GravityGPU.kext
sudo cp -R "$KEXT_DIR" /mnt/oc_efi/EFI/OC/Kexts/

echo "[*] Patching config.plist to inject GravityGPU.kext..."
sudo python3 -c "
import plistlib
with open('$PLIST', 'rb') as f:
    pl = plistlib.load(f)

# Check if already added
found = False
for kext in pl['Kernel']['Add']:
    if kext.get('BundlePath') == 'GravityGPU.kext':
        found = True
        break

if not found:
    pl['Kernel']['Add'].append({
        'Arch': 'x86_64',
        'BundlePath': 'GravityGPU.kext',
        'Comment': 'GravityGPU Guest Driver',
        'Enabled': True,
        'ExecutablePath': 'Contents/MacOS/GravityGPU',
        'MaxKernel': '',
        'MinKernel': '',
        'PlistPath': 'Contents/Info.plist'
    })
    with open('$PLIST', 'wb') as f:
        plistlib.dump(pl, f)
    print('    Added GravityGPU.kext to config.plist!')
else:
    print('    GravityGPU.kext is already in config.plist!')
"

echo "[*] Cleaning up..."
sudo umount /mnt/oc_efi
sudo qemu-nbd -d /dev/nbd0
echo "Done! The kext is injected. Now start the VM and it will load automatically."
