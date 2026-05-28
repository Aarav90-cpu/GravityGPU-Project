#!/usr/bin/env bash
set -e

SSH_PORT=2222
SSH_TARGET="aarav@localhost"
SSH_OPTS="-o StrictHostKeyChecking=no"
REMOTE_DIR="/tmp/gravitygpu"

echo "[*] Copying guest-userspace to VM..."
ssh $SSH_OPTS -p "$SSH_PORT" "$SSH_TARGET" "mkdir -p $REMOTE_DIR/guest-userspace"
scp $SSH_OPTS -P "$SSH_PORT" -r guest-userspace/* "$SSH_TARGET:$REMOTE_DIR/guest-userspace/"

echo "[*] Building test app on VM..."
ssh -t $SSH_OPTS -p "$SSH_PORT" "$SSH_TARGET" "
    cd $REMOTE_DIR/guest-userspace &&
    make clean && make
"

echo "[*] Running test app on VM..."
ssh -t $SSH_OPTS -p "$SSH_PORT" "$SSH_TARGET" "
    cd $REMOTE_DIR/guest-userspace &&
    ./GravityGPU_TestApp
"
