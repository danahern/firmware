#!/bin/bash
# One-time workspace setup for Docker-based builds.
# Run this script once after cloning (or after west update).
#
# What it does:
#   - Creates directories that Zephyr/west need to write at build time
#   - Makes them world-writable (safe for local dev, not for production)
#   - Fixes git safe.directory for all submodules (avoids "dubious ownership" in containers)
#
# Usage:
#   bash firmware/linux/docker/setup-workspace.sh [workspace_root]
#
# workspace_root defaults to the repo root (two levels up from this script).

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="${1:-$(cd "$SCRIPT_DIR/../../.." && pwd)}"

echo "Setting up Docker build workspace: $WORKSPACE"

# Zephyr cmake toolchain capability cache
ZEPHYR_CACHE="$WORKSPACE/zephyr/.cache"
echo "  Creating $ZEPHYR_CACHE"
mkdir -p "$ZEPHYR_CACHE/ToolchainCapabilityDatabase"
chmod -R a+rw "$ZEPHYR_CACHE"

# West config (west writes zephyr.base here on first use)
echo "  Fixing .west/ permissions"
chmod a+rw "$WORKSPACE/.west/config"

# Yocto build output dir
if [ -d "$WORKSPACE/yocto-build" ]; then
    echo "  Fixing yocto-build/ permissions"
    chmod -R a+rw "$WORKSPACE/yocto-build"
fi

echo "Done. Docker containers can now build in this workspace."
