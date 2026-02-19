# Cross-compilation Makefile for display_test
#
# BOARD selects the target:
#   (default)      STM32MP1 Cortex-A7 via Buildroot toolchain
#   alif-e7        Alif E7 Cortex-A32 via system cross-compiler
#
# Links against libdrm for DRM/KMS support.
# Falls back to raw framebuffer if DRM is unavailable at runtime.

ifeq ($(BOARD),alif-e7)
  # System cross-compiler (crossbuild-essential-armhf)
  CROSS_COMPILE ?= arm-linux-gnueabihf-
  CFLAGS  = -Wall -O2 -mcpu=cortex-a32 -mfpu=neon -mfloat-abi=hard
else
  # Buildroot toolchain path (inside container after build)
  TOOLCHAIN_DIR ?= /home/builder/buildroot/output/host/bin
  CROSS_COMPILE ?= $(TOOLCHAIN_DIR)/arm-none-linux-gnueabihf-
  CFLAGS  = -Wall -O2 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
endif

CC      = $(CROSS_COMPILE)gcc
STRIP   = $(CROSS_COMPILE)strip
CFLAGS += $(shell $(CROSS_COMPILE)pkg-config --cflags libdrm 2>/dev/null || echo -I/usr/include/libdrm)
LDFLAGS = $(shell $(CROSS_COMPILE)pkg-config --libs libdrm 2>/dev/null || echo -ldrm)

TARGET  = display_test
SRCS    = main.c
OBJS    = $(SRCS:.c=.o)

ARTIFACTS_DIR ?= /artifacts

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	$(STRIP) $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	cp $(TARGET) $(ARTIFACTS_DIR)/

clean:
	rm -f $(TARGET) $(OBJS)
