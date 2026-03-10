#!/bin/sh
# USB CDC-ECM gadget — Ethernet over USB
# Provides network interface for SSH/development access
# Optionally adds ADB via FunctionFS if adbd is installed

GADGET=/sys/kernel/config/usb_gadget/g1

# Find UDC
UDC_NAME=$(ls /sys/class/udc/ 2>/dev/null | head -n 1)
if [ -z "$UDC_NAME" ]; then
    echo "usb-gadget: No UDC found"
    exit 1
fi

# Detect board from device tree model string
if grep -qi "e8" /proc/device-tree/model 2>/dev/null; then
    SERIAL="eai-alif-e8-001"
    PRODUCT="Alif E8 Dev Board"
    DEV_MAC="02:00:86:e8:00:01"
    HOST_MAC="02:00:86:e8:00:02"
elif grep -qi "alif\|ensemble\|appkit-e\|devkit-e" /proc/device-tree/model 2>/dev/null; then
    SERIAL="eai-alif-e7-001"
    PRODUCT="Alif E7 Dev Board"
    DEV_MAC="02:00:86:e7:00:01"
    HOST_MAC="02:00:86:e7:00:02"
elif grep -qi "stm32mp" /proc/device-tree/model 2>/dev/null; then
    SERIAL="eai-stm32mp1-001"
    PRODUCT="STM32MP1 Dev Board"
    DEV_MAC="02:00:86:a1:00:01"
    HOST_MAC="02:00:86:a1:00:02"
else
    SERIAL="eai-generic-001"
    PRODUCT="EAI Dev Board"
    DEV_MAC="02:00:86:00:00:01"
    HOST_MAC="02:00:86:00:00:02"
fi

ADB_ENABLED=0
if [ -x /usr/bin/adbd ]; then
    ADB_ENABLED=1
fi

# Load modules (no-op if built-in)
modprobe libcomposite 2>/dev/null
modprobe usb_f_ecm 2>/dev/null
[ "$ADB_ENABLED" = "1" ] && modprobe usb_f_fs 2>/dev/null

# Mount configfs
mount -t configfs none /sys/kernel/config 2>/dev/null

# Create gadget
mkdir -p $GADGET
echo 0x1d6b > $GADGET/idVendor   # Linux Foundation
echo 0x0104 > $GADGET/idProduct  # Multifunction Composite Gadget
echo 0x0100 > $GADGET/bcdDevice
echo 0x0200 > $GADGET/bcdUSB

# Strings
mkdir -p $GADGET/strings/0x409
echo "$SERIAL" > $GADGET/strings/0x409/serialnumber
echo "EAI" > $GADGET/strings/0x409/manufacturer
echo "$PRODUCT" > $GADGET/strings/0x409/product

# Create ALL functions before linking any to config
mkdir -p $GADGET/functions/ecm.usb0
echo "$DEV_MAC" > $GADGET/functions/ecm.usb0/dev_addr
echo "$HOST_MAC" > $GADGET/functions/ecm.usb0/host_addr

if [ "$ADB_ENABLED" = "1" ]; then
    mkdir -p $GADGET/functions/ffs.adb
fi

# Configuration — link all functions
mkdir -p $GADGET/configs/c.1/strings/0x409
echo 250 > $GADGET/configs/c.1/MaxPower
ln -sf $GADGET/functions/ecm.usb0 $GADGET/configs/c.1/

if [ "$ADB_ENABLED" = "1" ]; then
    ln -sf $GADGET/functions/ffs.adb $GADGET/configs/c.1/
    echo "CDC-ECM + ADB" > $GADGET/configs/c.1/strings/0x409/configuration

    # Mount FunctionFS for adbd
    mkdir -p /dev/usb-ffs/adb
    mount -t functionfs adb /dev/usb-ffs/adb

    # Ensure devpts is mounted (adbd needs /dev/pts for shell)
    mount -t devpts devpts /dev/pts 2>/dev/null

    # Start adbd — it writes USB descriptors to ep0
    /usr/bin/adbd &

    # Wait for adbd to write descriptors (ep1 appears when ready)
    TIMEOUT=5
    while [ ! -e /dev/usb-ffs/adb/ep1 ] && [ $TIMEOUT -gt 0 ]; do
        sleep 1
        TIMEOUT=$((TIMEOUT - 1))
    done

    if [ -e /dev/usb-ffs/adb/ep1 ]; then
        echo "usb-gadget: ADB ready"
    else
        echo "usb-gadget: ADB timeout, continuing without ADB"
    fi
else
    echo "CDC-ECM" > $GADGET/configs/c.1/strings/0x409/configuration
fi

# Bind UDC (after adbd writes descriptors, if applicable)
echo "$UDC_NAME" > $GADGET/UDC

# Configure network interface
ifconfig usb0 192.168.55.2 netmask 255.255.255.0 up 2>/dev/null

echo "usb-gadget: CDC-ECM active on $UDC_NAME ($PRODUCT)"
echo "usb-gadget: Device IP: 192.168.55.2 (host should be 192.168.55.1)"
