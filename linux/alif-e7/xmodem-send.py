#!/usr/bin/env python3
"""
XMODEM-1K sender for Alif USB-to-OSPI flasher.

Sends a binary image to the flasher firmware over USB CDC-ACM using
XMODEM-1K protocol (1024-byte blocks with CRC-16).

No external dependencies beyond pyserial.
"""

import argparse
import glob
import os
import struct
import sys
import time

import serial

# XMODEM constants
SOH = 0x01   # 128-byte block header
STX = 0x02   # 1024-byte block header
EOT = 0x04   # End of transmission
ACK = 0x06   # Acknowledge
NAK = 0x15   # Negative acknowledge
CAN = 0x18   # Cancel
CRC_MODE = ord('C')  # CRC mode request

BLOCK_SIZE = 128  # Standard XMODEM (firmware doesn't support 1K)


def crc16_ccitt(data: bytes) -> int:
    """CRC-16 CCITT (same as flasher firmware)."""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc


def find_cdc_device() -> str:
    """Find the Alif USB CDC-ACM device on macOS."""
    # CDC-ACM devices appear as /dev/cu.usbmodem* on macOS
    candidates = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not candidates:
        return ""
    if len(candidates) == 1:
        return candidates[0]
    # Multiple devices — filter out known FTDI serial ports
    print(f"Multiple USB modem devices found:")
    for i, d in enumerate(candidates):
        print(f"  [{i}] {d}")
    return candidates[0]


def xmodem_send(port: serial.Serial, filepath: str) -> bool:
    """Send a file via XMODEM-1K with CRC-16."""
    file_size = os.path.getsize(filepath)
    total_blocks = (file_size + BLOCK_SIZE - 1) // BLOCK_SIZE

    print(f"Waiting for receiver ready signal...")

    # Wait for 'C' (CRC mode) or NAK from receiver
    deadline = time.time() + 30
    mode = None
    while time.time() < deadline:
        b = port.read(1)
        if not b:
            continue
        if b[0] == CRC_MODE:
            mode = "crc"
            break
        elif b[0] == NAK:
            mode = "checksum"
            break
        else:
            # Flasher might print status text before XMODEM starts
            sys.stdout.write(b.decode("ascii", errors="replace"))
            sys.stdout.flush()

    if mode is None:
        print("\nError: no response from receiver (timeout 30s)")
        print("Is the flasher firmware running? Check USB connection.")
        return False

    print(f"Receiver ready (mode: {mode})")
    print(f"Sending {file_size} bytes in {total_blocks} blocks...")

    with open(filepath, "rb") as f:
        block_num = 1
        start_time = time.time()

        while True:
            data = f.read(BLOCK_SIZE)
            if not data:
                break

            # Pad last block with 0xFF (erased flash value)
            if len(data) < BLOCK_SIZE:
                data += b'\xFF' * (BLOCK_SIZE - len(data))

            # Build packet: SOH/STX + block_num + ~block_num + data + CRC16
            seq = block_num & 0xFF
            header = STX if BLOCK_SIZE == 1024 else SOH
            if mode == "crc":
                crc = crc16_ccitt(data)
                packet = bytes([header, seq, 0xFF - seq]) + data + struct.pack(">H", crc)
            else:
                checksum = sum(data) & 0xFF
                packet = bytes([header, seq, 0xFF - seq]) + data + bytes([checksum])

            # Send and wait for ACK, retry up to 10 times
            for attempt in range(10):
                port.write(packet)
                port.flush()

                resp = port.read(1)
                if resp and resp[0] == ACK:
                    break
                elif resp and resp[0] == CAN:
                    print(f"\nReceiver cancelled at block {block_num}")
                    return False
                else:
                    if attempt < 9:
                        continue
                    print(f"\nNo ACK after 10 retries at block {block_num}")
                    return False

            # Progress
            elapsed = time.time() - start_time
            bytes_sent = block_num * BLOCK_SIZE
            speed = bytes_sent / elapsed if elapsed > 0 else 0
            pct = min(100, bytes_sent * 100 // file_size)
            eta = (file_size - bytes_sent) / speed if speed > 0 else 0
            sys.stdout.write(
                f"\r  [{pct:3d}%] {bytes_sent // 1024}/{file_size // 1024} KB"
                f"  {speed / 1024:.1f} KB/s  ETA {int(eta)}s"
                f"  block {block_num}/{total_blocks}"
            )
            sys.stdout.flush()

            block_num += 1

    # Send EOT
    for _ in range(5):
        port.write(bytes([EOT]))
        port.flush()
        resp = port.read(1)
        if resp and resp[0] == ACK:
            break

    elapsed = time.time() - start_time
    speed = file_size / elapsed if elapsed > 0 else 0
    print(f"\n\nTransfer complete: {file_size} bytes in {elapsed:.1f}s ({speed / 1024:.1f} KB/s)")
    return True


def read_status(port: serial.Serial, timeout: float = 5.0):
    """Read any post-transfer status messages from the flasher."""
    print("Reading flasher status...")
    deadline = time.time() + timeout
    while time.time() < deadline:
        data = port.read(port.in_waiting or 1)
        if data:
            sys.stdout.write(data.decode("ascii", errors="replace"))
            sys.stdout.flush()
            deadline = time.time() + 2  # extend on activity


def main():
    parser = argparse.ArgumentParser(description="XMODEM-1K sender for Alif USB-to-OSPI flasher")
    parser.add_argument("image", help="Binary image to send")
    parser.add_argument("--device", help="Serial device (auto-detected if omitted)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    args = parser.parse_args()

    if not os.path.isfile(args.image):
        print(f"Error: file not found: {args.image}")
        sys.exit(1)

    # Find device
    device = args.device or find_cdc_device()
    if not device:
        print("Error: no USB CDC-ACM device found")
        print("Expected /dev/cu.usbmodem* — is the board powered and flasher running?")
        sys.exit(1)

    print(f"Device: {device}")

    try:
        port = serial.Serial(device, args.baud, timeout=1)
    except serial.SerialException as e:
        print(f"Error opening {device}: {e}")
        sys.exit(1)

    try:
        if not xmodem_send(port, args.image):
            sys.exit(1)
        read_status(port)
    except KeyboardInterrupt:
        print("\nCancelled")
        port.write(bytes([CAN, CAN, CAN]))
        sys.exit(1)
    finally:
        port.close()

    print("\nDone. Flash normal boot config and power cycle to boot Linux.")


if __name__ == "__main__":
    main()
