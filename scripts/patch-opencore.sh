#!/usr/bin/env bash
set -e

if pgrep qemu-system-x86_64 > /dev/null; then
    echo "ERROR: QEMU is currently running! Please shut down the macOS VM first."
    exit 1
fi

OC_IMAGE="/mnt/OneClick-macOS-Simple-KVM/OpenCore.qcow2"
if [ ! -f "$OC_IMAGE" ]; then
    echo "ERROR: OpenCore.qcow2 not found at $OC_IMAGE"
    exit 1
fi

echo "[*] Loading NBD kernel module..."
sudo modprobe nbd max_part=8

echo "[*] Connecting OpenCore.qcow2 to /dev/nbd0..."
sudo qemu-nbd -d /dev/nbd0 2>/dev/null || true
sudo qemu-nbd -c /dev/nbd0 "$OC_IMAGE"
sleep 1 # wait for partitions to be populated

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
    echo "ERROR: config.plist not found in any partition!"
    sudo qemu-nbd -d /dev/nbd0
    exit 1
fi

echo "[*] Patching config.plist with python3..."
sudo python3 -c "
import plistlib
with open('$PLIST', 'rb') as f:
    pl = plistlib.load(f)
try:
    boot_args = pl['NVRAM']['Add']['7C436110-AB2A-4BBB-A880-FE41995C9F82']['boot-args']
    if 'amfi_get_out_of_my_way' not in boot_args:
        pl['NVRAM']['Add']['7C436110-AB2A-4BBB-A880-FE41995C9F82']['boot-args'] = boot_args + ' debug=0x100 amfi_get_out_of_my_way=1 amfi_allow_any_signature=1'
        with open('$PLIST', 'wb') as f:
            plistlib.dump(pl, f)
        print('    Successfully patched boot-args!')
    else:
        print('    boot-args already patched!')
except KeyError as e:
    print('    KeyError: ', e)
"

echo "[*] Cleaning up..."
sudo umount /mnt/oc_efi
sudo qemu-nbd -d /dev/nbd0

echo "Done! You can now start the VM."
