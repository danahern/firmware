#!/bin/bash
# Cross-compile PicoClaw for embedded ARM targets
# Expects /src mounted to PicoClaw source, /out for output binary
set -euo pipefail

ARCH="${1:-armv7}"
SRC="/src"
OUT="/out"

if [ ! -f "$SRC/go.mod" ]; then
    echo "Error: PicoClaw source not found at $SRC"
    echo "Mount the picoclaw repo to /src"
    exit 1
fi

cd "$SRC"

VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo "dev")
GIT_COMMIT=$(git rev-parse --short=8 HEAD 2>/dev/null || echo "dev")
BUILD_TIME=$(date +%FT%T%z)
GO_VERSION=$(go version | awk '{print $3}')
LDFLAGS="-X main.version=${VERSION} -X main.gitCommit=${GIT_COMMIT} -X main.buildTime=${BUILD_TIME} -X main.goVersion=${GO_VERSION} -s -w"

export CGO_ENABLED=0 GOOS=linux

case "$ARCH" in
    armv7)
        export GOARCH=arm GOARM=7
        OUTPUT="picoclaw-linux-armv7"
        ;;
    arm64)
        export GOARCH=arm64
        OUTPUT="picoclaw-linux-arm64"
        ;;
    *)
        echo "Unknown architecture: $ARCH (supported: armv7, arm64)"
        exit 1
        ;;
esac

echo "Building PicoClaw for linux/${GOARCH}..."
go generate ./...
go build -tags stdjson -ldflags "$LDFLAGS" -o "${OUT}/${OUTPUT}" ./cmd/picoclaw

SIZE=$(ls -lh "${OUT}/${OUTPUT}" | awk '{print $5}')
echo "Built: ${OUTPUT} (${SIZE})"
