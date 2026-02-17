# Linux Applications for STM32MP1 Cortex-A7

Userspace applications for the STM32MP1 A7 core, cross-compiled using Buildroot's toolchain.

## Apps

| App | Description |
|-----|-------------|
| `hello` | Print system info (kernel version, machine type) |
| `rpmsg_echo` | RPMsg echo client — send messages to M4 core via `/dev/rpmsg_charN` |

## Building

Apps are cross-compiled inside the Buildroot Docker container using the linux-build MCP.

### Prerequisites

1. Build the Docker image (see `docker/Dockerfile`)
2. Complete a Buildroot build (produces the cross-toolchain in `/buildroot/output/host/bin/`)

### MCP Workflow

```
# Start container with workspace mounted
linux-build.start_container(name="stm32mp1-build", image="stm32mp1-sdk", workspace_dir="/path/to/work")

# Build all apps (after Buildroot build has completed)
linux-build.build(container="stm32mp1-build", command="make -C /workspace/firmware/linux/apps all install", workdir="/workspace")

# Collect binaries
linux-build.collect_artifacts(container="stm32mp1-build", host_path="/tmp/stm32mp1-apps")

# Deploy to board
linux-build.deploy(file_path="/tmp/stm32mp1-apps/hello", board_ip="192.168.1.100")

# Run on board
linux-build.ssh_command(command="/home/root/hello", board_ip="192.168.1.100")
```

### Manual Cross-Compilation

If you have a cross-toolchain on the host:

```bash
CROSS_COMPILE=arm-linux-gnueabihf- make -C apps/hello
```

## RPMsg Echo

The `rpmsg_echo` app communicates with the M4 core via RPMsg. Prerequisites:

1. M4 firmware with an RPMsg echo endpoint (loaded via remoteproc or OpenOCD)
2. `rpmsg_char` kernel module loaded on A7
3. `/dev/rpmsg_ctrl0` device exists

```bash
# On the board
modprobe rpmsg_char
./rpmsg_echo -n 5 "Hello M4"
```

## Directory Structure

```
linux/
├── apps/
│   ├── Makefile          # Top-level: builds all apps
│   ├── hello/            # Simple hello world
│   │   ├── main.c
│   │   └── Makefile
│   └── rpmsg_echo/       # RPMsg IPC client
│       ├── main.c
│       └── Makefile
├── docker/               # Build environment (managed by other session)
│   └── Dockerfile
└── README.md             # This file
```
