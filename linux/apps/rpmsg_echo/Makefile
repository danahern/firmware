# Cross-compilation Makefile for STM32MP1 RPMsg Echo Client
#
# Uses Buildroot's toolchain from the build container.

TOOLCHAIN_DIR ?= /buildroot/output/host/bin
CROSS_COMPILE ?= $(TOOLCHAIN_DIR)/arm-linux-gnueabihf-

CC      = $(CROSS_COMPILE)gcc
STRIP   = $(CROSS_COMPILE)strip
CFLAGS  = -Wall -O2 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
LDFLAGS =

TARGET  = rpmsg_echo
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
