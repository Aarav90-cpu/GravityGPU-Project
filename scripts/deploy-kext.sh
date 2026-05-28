#!/bin/bash
# scripts/deploy-kext.sh
# Deploys the GravityGPU guest driver to the macOS VM via SSH.
#
# Prerequisites:
#   1. macOS VM is running with SSH port forwarded (port 2222)
#   2. Remote Login enabled in macOS: System Preferences → Sharing → Remote Login
#   3. SIP disabled (csrutil disable from Recovery Mode)
#
# Usage:
#   ./scripts/deploy-kext.sh [user@host:port]
#   ./scripts/deploy-kext.sh                     # defaults to aarav@localhost:2222

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE_DIR="$(realpath "$SCRIPT_DIR/..")"
SSH_TARGET="${1:-aarav@localhost}"
SSH_PORT="${2:-2222}"
REMOTE_DIR="/tmp/gravitygpu"

SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5"

echo "═══════════════════════════════════════════"
echo "  GravityGPU Kext Deployment"
echo "═══════════════════════════════════════════"
echo "  Target: $SSH_TARGET (port $SSH_PORT)"
echo ""

# ── Test SSH connectivity ──
echo "[*] Testing SSH connection..."
if ! ssh $SSH_OPTS -p "$SSH_PORT" "$SSH_TARGET" "echo 'SSH OK'" 2>/dev/null; then
    echo ""
    echo "ERROR: Cannot connect to VM via SSH."
    echo ""
    echo "Make sure:"
    echo "  1. The macOS VM is running"
    echo "  2. Remote Login is enabled in System Preferences → Sharing"
    echo "  3. Port forwarding is active (port $SSH_PORT → guest port 22)"
    echo ""
    echo "You can also copy files manually:"
    echo "  1. Share the guest-driver folder via a shared folder or USB"
    echo "  2. Inside macOS, run: cd /path/to/guest-driver && ./build-kext.sh"
    echo "  3. Then: sudo kextload build/GravityGPU.kext"
    exit 1
fi

# Ensure directory exists first using an explicit ssh command that we verify
ssh $SSH_OPTS -p "$SSH_PORT" "$SSH_TARGET" "mkdir -p $REMOTE_DIR"

# Copy guest driver
scp $SSH_OPTS -P "$SSH_PORT" -r \
    "$WORKSPACE_DIR/guest-driver" \
    "$SSH_TARGET:$REMOTE_DIR/"

# Copy protocol (shared headers needed for build)
scp $SSH_OPTS -P "$SSH_PORT" -r \
    "$WORKSPACE_DIR/protocol" \
    "$SSH_TARGET:$REMOTE_DIR/"

# ── Build & Load on VM ──
echo "[*] Building and loading kext inside VM (might prompt for password)..."
ssh -t $SSH_OPTS -p "$SSH_PORT" "$SSH_TARGET" "
    set -e
    echo '[*] Building...'
    cd $REMOTE_DIR/guest-driver
    chmod +x build-kext.sh
    ./build-kext.sh
    
    echo '[*] Unloading old kext...'
    sudo kextunload build/GravityGPU.kext 2>/dev/null || true
    
    echo '[*] Loading new kext...'
    sudo kextload -v build/GravityGPU.kext
    
    echo '[*] Verifying...'
    kextstat | grep -i gravity || echo 'WARNING: kext not in kextstat'
    
    echo '[*] Checking dmesg...'
    dmesg | grep -i gravity | tail -20 || echo '(no messages yet)'
"

echo "═══════════════════════════════════════════"
echo "  Deployment complete!"
echo "═══════════════════════════════════════════"
