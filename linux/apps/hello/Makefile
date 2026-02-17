# Cross-compilation Makefile for STM32MP1 Cortex-A7
#
# Uses Buildroot's toolchain from the build container.
# Set CROSS_COMPILE or TOOLCHAIN_DIR to override.

# Buildroot toolchain path (inside container after build)
TOOLCHAIN_DIR ?= /buildroot/output/host/bin
CROSS_COMPILE ?= $(TOOLCHAIN_DIR)/arm-linux-gnueabihf-

CC      = $(CROSS_COMPILE)gcc
STRIP   = $(CROSS_COMPILE)strip
CFLAGS  = -Wall -O2 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
LDFLAGS =

TARGET  = hello
SRCS    = main.c
OBJS    = $(SRCS:.c=.o)

ARTIFACTS_DIR ?= /artifacts

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	$(STRIP) $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	cp $(TARGET) $(ARTIFACTS_DIR)/

clean:
	rm -f $(TARGET) $(OBJS)
