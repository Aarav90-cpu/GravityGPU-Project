#!/bin/bash
# guest-driver/build-kext.sh
# Builds the GravityGPU.kext from source inside the macOS VM.
#
# Requirements:
#   - Xcode Command Line Tools: xcode-select --install
#   - OR a Kernel Development Kit (KDK) from https://developer.apple.com/download/all/
#
# Usage:
#   cd /path/to/guest-driver && ./build-kext.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/GravityGPU"
PROTO_DIR="$SCRIPT_DIR/../protocol"
BUILD_DIR="$SCRIPT_DIR/build"
KEXT_DIR="$BUILD_DIR/GravityGPU.kext"

# ── Detect SDK ──
KERN_FRAMEWORK="/System/Library/Frameworks/Kernel.framework"
if [ ! -d "$KERN_FRAMEWORK/Headers" ]; then
    # Try MacOSX SDK path via xcrun
    SDK_PATH=$(xcrun --show-sdk-path 2>/dev/null || echo "")
    if [ -n "$SDK_PATH" ] && [ -d "$SDK_PATH/System/Library/Frameworks/Kernel.framework/Headers" ]; then
        KERN_FRAMEWORK="$SDK_PATH/System/Library/Frameworks/Kernel.framework"
    else
        # Try KDK path
        KDK_DIR=$(ls -d /Library/Developer/KDKs/KDK_*.kdk 2>/dev/null | tail -1)
        if [ -n "$KDK_DIR" ] && [ -d "$KDK_DIR/System/Library/Frameworks/Kernel.framework/Headers" ]; then
            KERN_FRAMEWORK="$KDK_DIR/System/Library/Frameworks/Kernel.framework"
        else
            echo "ERROR: Cannot find Kernel.framework/Headers. Install Xcode CLI tools or a KDK."
            exit 1
        fi
    fi
fi
echo "[*] Using Kernel.framework at: $KERN_FRAMEWORK"

# ── Compiler flags for kernel extension ──
ARCH=$(uname -m)
CLANG_BUILTIN_INC=$(clang -print-file-name=include)

KEXT_CFLAGS=(
    -target ${ARCH}-apple-macos10.15
    -nostdinc
    -isystem "$KERN_FRAMEWORK/Headers"
    -isystem "$CLANG_BUILTIN_INC"
    -I"$SRC_DIR"
    -I"$PROTO_DIR/include"
    -DKERNEL
    -DKERNEL_PRIVATE
    -D__APPLE__
    -DAPPLE
    -DNeXT
    -fno-builtin
    -fno-common
    -fno-exceptions
    -fno-rtti
    -mkernel
    -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations
    -O2
)

KEXT_LDFLAGS=(
    -target ${ARCH}-apple-macos10.15
    -nostdlib
    -Xlinker -kext
    -Xlinker -export_dynamic
    -lkmod
    -lcc_kext
)

# ── Clean ──
sudo rm -rf "$BUILD_DIR"
mkdir -p "$KEXT_DIR/Contents/MacOS"

# ── Compile protocol library ──
echo "[*] Compiling protocol library..."
clang "${KEXT_CFLAGS[@]}" -c "$PROTO_DIR/src/gravity_protocol.c" -o "$BUILD_DIR/gravity_protocol.o"
clang "${KEXT_CFLAGS[@]}" -c "$PROTO_DIR/src/gravity_formats.c" -o "$BUILD_DIR/gravity_formats.o"

# ── Compile driver ──
echo "[*] Compiling GravityGPUAccelerator..."
clang++ "${KEXT_CFLAGS[@]}" -c "$SRC_DIR/GravityGPUAccelerator.cpp" -o "$BUILD_DIR/GravityGPUAccelerator.o"

echo "[*] Compiling GravityGPUUserClient..."
clang++ "${KEXT_CFLAGS[@]}" -c "$SRC_DIR/GravityGPUUserClient.cpp" -o "$BUILD_DIR/GravityGPUUserClient.o"

# ── Link ──
echo "[*] Linking GravityGPU.kext..."
clang++ "${KEXT_LDFLAGS[@]}" \
    "$BUILD_DIR/GravityGPUAccelerator.o" \
    "$BUILD_DIR/GravityGPUUserClient.o" \
    "$BUILD_DIR/gravity_protocol.o" \
    "$BUILD_DIR/gravity_formats.o" \
    -o "$KEXT_DIR/Contents/MacOS/GravityGPU"

# ── Create bundle ──
echo "[*] Creating kext bundle..."
cp "$SRC_DIR/Info.plist" "$KEXT_DIR/Contents/Info.plist"

# ── Ad-hoc Sign ──
echo "[*] Ad-hoc signing the kext..."
sudo codesign --force --deep --sign - "$KEXT_DIR"

# ── Set permissions ──
echo "[*] Setting permissions..."
sudo chown -R root:wheel "$KEXT_DIR"
sudo chmod -R 755 "$KEXT_DIR"

echo ""
echo "═══════════════════════════════════════════"
echo "  GravityGPU.kext built successfully!"
echo "  Location: $KEXT_DIR"
echo "═══════════════════════════════════════════"
echo ""
echo "To load:  sudo kextload $KEXT_DIR"
echo "To check: kextstat | grep GravityGPU"
echo "To log:   log show --predicate 'senderImagePath contains \"GravityGPU\"' --last 5m"
echo ""
