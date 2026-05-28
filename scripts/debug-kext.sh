#!/usr/bin/env bash
SSH_PORT=2222
SSH_TARGET="aarav@localhost"
SSH_OPTS="-o StrictHostKeyChecking=no"

echo "[*] Checking why the kext isn't loading..."
ssh -t $SSH_OPTS -p "$SSH_PORT" "$SSH_TARGET" "
    sudo kextutil -t /tmp/gravitygpu/guest-driver/build/GravityGPU.kext
"
