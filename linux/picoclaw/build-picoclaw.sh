#!/bin/bash
# Build PicoClaw Go binary for ARM targets
#
# Usage:
#   ./build-picoclaw.sh armv7    # STM32MP1 (Cortex-A7) / Alif E7 (Cortex-A32)
#   ./build-picoclaw.sh arm64    # RPi 4/5, RK3576
#   ./build-picoclaw.sh all      # Both architectures
#
# Output: build/picoclaw-linux-{armv7,arm64}
#
# The binary is then copied to the Yocto recipe files/ directory:
#   cp build/picoclaw-linux-armv7 ../yocto/meta-eai/recipes-apps/picoclaw/files/picoclaw

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE_ROOT="${SCRIPT_DIR}/../../.."
PICOCLAW_SRC="${WORKSPACE_ROOT}/picoclaw"
BUILD_DIR="${SCRIPT_DIR}/build"

if [ ! -d "$PICOCLAW_SRC" ]; then
    echo "Error: PicoClaw source not found at $PICOCLAW_SRC"
    echo "Clone it first: git clone https://github.com/sipeed/picoclaw ${WORKSPACE_ROOT}/picoclaw"
    exit 1
fi

VERSION=$(cd "$PICOCLAW_SRC" && git describe --tags --always --dirty 2>/dev/null || echo "dev")
GIT_COMMIT=$(cd "$PICOCLAW_SRC" && git rev-parse --short=8 HEAD 2>/dev/null || echo "dev")
BUILD_TIME=$(date +%FT%T%z)
GO_VERSION=$(go version | awk '{print $3}')
LDFLAGS="-X main.version=${VERSION} -X main.gitCommit=${GIT_COMMIT} -X main.buildTime=${BUILD_TIME} -X main.goVersion=${GO_VERSION} -s -w"

build_arch() {
    local arch="$1"
    local goarch goarm output_name

    case "$arch" in
        armv7)
            goarch="arm"
            goarm="7"
            output_name="picoclaw-linux-armv7"
            ;;
        arm64)
            goarch="arm64"
            goarm=""
            output_name="picoclaw-linux-arm64"
            ;;
        *)
            echo "Unknown architecture: $arch"
            echo "Supported: armv7, arm64, all"
            exit 1
            ;;
    esac

    echo "Building PicoClaw for linux/${goarch}..."
    mkdir -p "$BUILD_DIR"

    (
        cd "$PICOCLAW_SRC"
        export CGO_ENABLED=0 GOOS=linux GOARCH="$goarch"
        [ -n "$goarm" ] && export GOARM="$goarm"
        # Generate embedded assets
        go generate ./...
        # Cross-compile
        go build -tags stdjson -ldflags "$LDFLAGS" \
            -o "${BUILD_DIR}/${output_name}" ./cmd/picoclaw
    )

    local size
    size=$(ls -lh "${BUILD_DIR}/${output_name}" | awk '{print $5}')
    echo "Built: ${BUILD_DIR}/${output_name} (${size})"
}

TARGET="${1:-armv7}"

case "$TARGET" in
    all)
        build_arch armv7
        build_arch arm64
        ;;
    *)
        build_arch "$TARGET"
        ;;
esac

echo ""
echo "To install into Yocto recipe:"
echo "  cp ${BUILD_DIR}/picoclaw-linux-armv7 ${SCRIPT_DIR}/../yocto/meta-eai/recipes-apps/picoclaw/files/picoclaw"
