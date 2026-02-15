#!/bin/bash
# Scaffold a new Zephyr application under apps/
#
# Usage: ./scripts/new-app.sh <app-name> [board]

set -e

APP_NAME="$1"
BOARD="$2"

if [ -z "$APP_NAME" ]; then
    echo "Usage: $0 <app-name> [board]"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APPS_DIR="$(cd "$SCRIPT_DIR/../apps" && pwd)"
APP_DIR="$APPS_DIR/$APP_NAME"

if [ -d "$APP_DIR" ]; then
    echo "Error: $APP_DIR already exists"
    exit 1
fi

mkdir -p "$APP_DIR/src"

# CMakeLists.txt
if [ -n "$BOARD" ]; then
    BOARD_COMMENT="# Default board: $BOARD"
else
    BOARD_COMMENT=""
fi

cat > "$APP_DIR/CMakeLists.txt" <<EOF
# SPDX-License-Identifier: Apache-2.0
${BOARD_COMMENT:+${BOARD_COMMENT}
}
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS \$ENV{ZEPHYR_BASE})
project($APP_NAME)

target_sources(app PRIVATE src/main.c)
EOF

# prj.conf
cat > "$APP_DIR/prj.conf" <<EOF
# Logging
CONFIG_LOG=y

# Stack sizes
CONFIG_MAIN_STACK_SIZE=2048
EOF

# src/main.c
cat > "$APP_DIR/src/main.c" <<EOF
/*
 * $APP_NAME
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER($APP_NAME, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("$APP_NAME starting");

	return 0;
}
EOF

echo "Created $APP_DIR/"
echo "  CMakeLists.txt"
echo "  prj.conf"
echo "  src/main.c"
if [ -n "$BOARD" ]; then
    echo ""
    echo "Build: zephyr-build.build(app=\"$APP_NAME\", board=\"$BOARD\", pristine=true)"
fi
