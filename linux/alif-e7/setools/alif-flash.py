#!/usr/bin/env python3
"""Alif E7 MRAM flash tool — maintenance mode entry + image write over SE-UART.

Consolidates the ISP protocol into one reliable tool. Replaces the 7 ad-hoc
isp_*.py scripts and works around the `maintenance` binary's wrong baud rate.

Usage:
    # Enter maintenance mode (power cycle board, then run within 5s):
    ./alif-flash.py maintenance

    # Flash all images from ATOC JSON config:
    ./alif-flash.py flash

    # Full flow: maintenance + flash:
    ./alif-flash.py flash --maintenance

    # Check if SE is responsive:
    ./alif-flash.py probe

    # Generate ATOC + flash (runs app-gen-toc first):
    ./alif-flash.py flash --gen-toc

Protocol: Alif SE-UART ISP at 57600 baud.
Packet format: [length, cmd, data..., checksum] where all bytes sum to 0 mod 256.
"""
import argparse
import json
import os
import struct
import subprocess
import sys
import time

try:
    import serial
except ImportError:
    print("Error: pyserial not installed. Run: pip3 install pyserial")
    sys.exit(1)

# --- ISP Protocol Constants ---
BAUD_RATE = 57600
DATA_PER_CHUNK = 240  # Max data bytes per DOWNLOAD_DATA packet

CMD_START_ISP = 0x00
CMD_STOP_ISP = 0x01
CMD_DOWNLOAD_DATA = 0x04
CMD_DOWNLOAD_DONE = 0x05
CMD_BURN_MRAM = 0x08
CMD_RESET_DEVICE = 0x09
CMD_ENQUIRY = 0x0F
CMD_SET_MAINTENANCE = 0x16
CMD_ACK = 0xFE
CMD_DATA_RESP = 0xFD


# --- Protocol Helpers ---

def calc_checksum(data):
    """All bytes including checksum must sum to 0 mod 256."""
    return (0 - sum(data)) & 0xFF


def make_packet(cmd, data=b''):
    """Build ISP packet: [length, cmd, data..., checksum]."""
    payload = bytes([cmd]) + data
    length = len(payload) + 2  # +1 for length byte, +1 for checksum
    pkt = bytes([length]) + payload
    pkt += bytes([calc_checksum(pkt)])
    return pkt


def read_response(ser, timeout=2):
    """Read one ISP response packet. Returns (cmd, data) or (None, b'')."""
    old_timeout = ser.timeout
    ser.timeout = timeout
    try:
        first = ser.read(1)
        if not first:
            return None, b''
        length = first[0]
        if length < 2:
            return None, b''
        rest = ser.read(length - 1)
        if len(rest) < 1:
            return None, b''
        cmd = rest[0]
        data = rest[1:-1] if len(rest) > 2 else b''
        return cmd, data
    finally:
        ser.timeout = old_timeout


def send_cmd(ser, cmd, data=b'', label="", quiet=False):
    """Send ISP command, read response. Returns (ok, resp_data)."""
    pkt = make_packet(cmd, data)
    ser.reset_input_buffer()
    ser.write(pkt)
    ser.flush()
    time.sleep(0.05)

    resp_cmd, resp_data = read_response(ser)
    ok = resp_cmd in (CMD_ACK, CMD_DATA_RESP)

    if not quiet:
        if resp_cmd == CMD_ACK:
            status = "ACK"
        elif resp_cmd == CMD_DATA_RESP:
            status = f"DATA ({len(resp_data)} bytes)"
        elif resp_cmd is not None:
            status = f"0x{resp_cmd:02X}"
        else:
            status = "no response"
        print(f"  {label}: {status}")

    return ok, resp_data


# --- Serial Port Discovery ---

def find_se_uart():
    """Find the Alif SE-UART port (JLink VCOM)."""
    import glob
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        print("Error: No /dev/cu.usbmodem* ports found.")
        print("Is the PRG_USB cable connected?")
        sys.exit(1)
    if len(ports) == 1:
        return ports[0]
    # Multiple ports — prefer the one with the JLink serial pattern
    # SE-UART is typically the first (lower number) port
    print(f"Found {len(ports)} ports: {', '.join(ports)}")
    print(f"Using first port: {ports[0]}")
    return ports[0]


def open_serial(port, retries=3, retry_delay=2):
    """Open serial port with retries (port may disappear during power cycle)."""
    for attempt in range(retries):
        try:
            ser = serial.Serial(port, BAUD_RATE, timeout=2)
            # Aggressive drain — read until empty with short timeout
            time.sleep(0.1)
            while ser.in_waiting:
                ser.read(ser.in_waiting)
                time.sleep(0.05)
            return ser
        except serial.SerialException as e:
            if attempt < retries - 1:
                print(f"  Port not ready, retrying in {retry_delay}s... ({e})")
                time.sleep(retry_delay)
            else:
                raise


def start_isp(ser, label="START_ISP", retries=3):
    """Send START_ISP with retries — handles stale data in buffer."""
    for attempt in range(retries):
        # Drain buffer before each attempt
        time.sleep(0.1)
        while ser.in_waiting:
            ser.read(ser.in_waiting)
            time.sleep(0.05)
        ok, data = send_cmd(ser, CMD_START_ISP, label=label,
                            quiet=(attempt < retries - 1))
        if ok:
            if attempt > 0:
                print(f"  {label}: ACK (attempt {attempt + 1})")
            return True, data
        if attempt < retries - 1:
            time.sleep(0.3)
    return False, b''


# --- Commands ---

def cmd_probe(port):
    """Check if the SE is responsive."""
    print(f"Probing SE-UART on {port} at {BAUD_RATE} baud...")
    ser = open_serial(port)
    try:
        ok, _ = start_isp(ser, label="START_ISP")
        if ok:
            print("SE is in ISP mode.")
            ok2, data = send_cmd(ser, CMD_ENQUIRY, label="ENQUIRY")
            if ok2 and len(data) >= 10:
                maint = data[9]
                print(f"Maintenance mode: {'YES' if maint else 'NO'}")
            send_cmd(ser, CMD_STOP_ISP, label="STOP_ISP", quiet=True)
            return True
        else:
            print("SE did not respond to START_ISP.")
            print("The board may need a power cycle, or SE is busy running firmware.")
            return False
    finally:
        ser.close()


def cmd_maintenance(port):
    """Enter maintenance mode via ISP protocol.

    Flow:
    1. START_ISP → SET_MAINTENANCE → STOP_ISP → RESET_DEVICE
    2. Wait for reboot
    3. Verify with ENQUIRY
    """
    print(f"Entering maintenance mode on {port}...")
    ser = open_serial(port)
    try:
        # Phase 1: Set maintenance flag
        ok, _ = start_isp(ser, label="START_ISP")
        if not ok:
            print("\nSE did not respond. Try:")
            print("  1. Unplug and replug PRG_USB cable")
            print("  2. Run this command within 2-3 seconds of plugging in")
            return False

        send_cmd(ser, CMD_SET_MAINTENANCE, label="SET_MAINTENANCE")
        send_cmd(ser, CMD_STOP_ISP, label="STOP_ISP")

        print("  Resetting device...")
        ser.write(make_packet(CMD_RESET_DEVICE))
        ser.flush()
        ser.close()

        # Phase 2: Wait for reboot and reconnect
        print("  Waiting for reboot (5s)...")
        time.sleep(5)

        ser = open_serial(port, retries=5, retry_delay=2)

        # Phase 3: Verify maintenance mode
        ok, _ = start_isp(ser, label="START_ISP (post-reset)")
        if not ok:
            print("Warning: SE not responding after reset.")
            print("Try power cycling (unplug/replug PRG_USB) and run again.")
            ser.close()
            return False

        ok, data = send_cmd(ser, CMD_ENQUIRY, label="ENQUIRY")
        if ok and len(data) >= 10 and data[9]:
            print("\nMaintenance mode: ACTIVE")
            send_cmd(ser, CMD_STOP_ISP, label="STOP_ISP", quiet=True)
            ser.close()
            return True
        else:
            maint = data[9] if ok and len(data) >= 10 else "unknown"
            print(f"\nWarning: Maintenance flag = {maint}")
            print("Proceeding anyway — MRAM write may still work.")
            send_cmd(ser, CMD_STOP_ISP, label="STOP_ISP", quiet=True)
            ser.close()
            return True  # Optimistic — let flash attempt proceed
    except Exception:
        ser.close()
        raise


def write_image(ser, path, addr):
    """Write a single image to MRAM at the given address."""
    with open(path, 'rb') as f:
        data = f.read()
    orig = len(data)
    pad = (16 - (orig % 16)) % 16
    data += b'\x00' * pad
    size = len(data)
    name = os.path.basename(path)
    print(f"\n  [{name}] {orig} bytes (padded to {size}) → 0x{addr:08X}")

    # BURN_MRAM
    ok, _ = send_cmd(ser, CMD_BURN_MRAM,
                     struct.pack('<II', addr, size), "BURN_MRAM")
    if not ok:
        print(f"  ERROR: BURN_MRAM rejected for {name}")
        return False

    # Send data chunks
    offset = 0
    chunk_num = 0
    total = (size + DATA_PER_CHUNK - 1) // DATA_PER_CHUNK
    t0 = time.time()

    while offset < size:
        chunk = data[offset:offset + DATA_PER_CHUNK]
        seq = struct.pack('<H', chunk_num)
        pkt = make_packet(CMD_DOWNLOAD_DATA, seq + chunk)
        ser.reset_input_buffer()
        ser.write(pkt)
        ser.flush()
        time.sleep(0.02)

        resp_cmd, _ = read_response(ser, timeout=1)
        if resp_cmd not in (CMD_ACK, CMD_DATA_RESP):
            status = f"0x{resp_cmd:02X}" if resp_cmd else "no response"
            print(f"  Chunk {chunk_num}/{total}: {status}")
            if chunk_num < total - 1:
                return False

        offset += len(chunk)
        chunk_num += 1
        if chunk_num % 50 == 0 or chunk_num == total:
            elapsed = time.time() - t0
            pct = 100 * offset // size
            print(f"  {chunk_num}/{total} ({pct}%) [{elapsed:.1f}s]")

    # DOWNLOAD_DONE
    send_cmd(ser, CMD_DOWNLOAD_DONE, label="DOWNLOAD_DONE")
    elapsed = time.time() - t0
    print(f"  Done: {size} bytes in {elapsed:.1f}s "
          f"({size / elapsed:.0f} B/s)")
    return True


def cmd_flash(port, config_path, setools_dir, gen_toc=False, maintenance=False):
    """Flash all images defined in the ATOC JSON config."""

    if maintenance:
        if not cmd_maintenance(port):
            print("Failed to enter maintenance mode. Aborting.")
            return False
        # Small delay after maintenance mode entry
        time.sleep(1)

    if gen_toc:
        print("\n=== Generating ATOC ===")
        rel_config = os.path.relpath(config_path, setools_dir)
        result = subprocess.run(
            ["./app-gen-toc", "-f", rel_config],
            cwd=setools_dir, capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"app-gen-toc failed:\n{result.stderr}")
            return False
        print(result.stdout)

    # Parse ATOC JSON for image list
    with open(config_path) as f:
        config = json.load(f)

    images_dir = os.path.join(setools_dir, "build", "images")
    images = []
    for key in ("TFA", "DTB", "KERNEL", "ROOTFS"):
        entry = config.get(key, {})
        if entry.get("disabled", False):
            continue
        binary = entry.get("binary")
        addr_str = entry.get("mramAddress")
        if binary and addr_str:
            path = os.path.join(images_dir, binary)
            addr = int(addr_str, 16)
            if not os.path.exists(path):
                print(f"Error: {path} not found")
                return False
            images.append((path, addr))

    if not images:
        print("Error: No images found in config")
        return False

    # Calculate total size
    total_bytes = sum(os.path.getsize(p) for p, _ in images)
    print(f"\n=== Flashing {len(images)} images ({total_bytes:,} bytes) ===")
    for path, addr in images:
        print(f"  {os.path.basename(path):45s} → 0x{addr:08X} "
              f"({os.path.getsize(path):>10,} bytes)")

    # Open serial and write
    print(f"\nConnecting to {port} at {BAUD_RATE} baud...")
    ser = open_serial(port)
    try:
        ok, _ = start_isp(ser, label="START_ISP")
        if not ok:
            print("Error: SE not responding. Is the board in maintenance mode?")
            print("Run: ./alif-flash.py maintenance")
            return False

        t0 = time.time()
        for path, addr in images:
            if not write_image(ser, path, addr):
                print(f"\nFAILED writing {os.path.basename(path)}")
                ser.close()
                return False

        total_time = time.time() - t0
        print(f"\n=== All images written ({total_time:.1f}s) ===")

        send_cmd(ser, CMD_STOP_ISP, label="STOP_ISP")
        ser.write(make_packet(CMD_RESET_DEVICE))
        ser.flush()
        print("Device resetting. Monitor UART2 at 115200 baud for Linux console.")
        print("NOTE: Full power cycle (unplug/replug PRG_USB) may be needed.")
        return True
    finally:
        ser.close()


# --- Main ---

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    workspace_root = os.path.abspath(os.path.join(script_dir, "../../../.."))
    setools_dir = os.path.join(workspace_root, "tools", "setools")
    default_config = os.path.join(setools_dir, "build", "config",
                                  "linux-boot-e7.json")

    parser = argparse.ArgumentParser(
        description="Alif E7 MRAM flash tool (SE-UART ISP protocol)")
    parser.add_argument("--port", help="Serial port (auto-detected if omitted)")

    sub = parser.add_subparsers(dest="command")

    sub.add_parser("probe", help="Check if SE is responsive")
    sub.add_parser("maintenance", help="Enter maintenance mode")

    flash_p = sub.add_parser("flash", help="Flash images to MRAM")
    flash_p.add_argument("--config", default=default_config,
                         help="ATOC JSON config path")
    flash_p.add_argument("--setools-dir", default=setools_dir,
                         help="SETOOLS directory")
    flash_p.add_argument("--gen-toc", action="store_true",
                         help="Run app-gen-toc before flashing")
    flash_p.add_argument("--maintenance", action="store_true",
                         help="Enter maintenance mode first")

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        sys.exit(1)

    port = args.port or find_se_uart()

    if args.command == "probe":
        sys.exit(0 if cmd_probe(port) else 1)

    elif args.command == "maintenance":
        sys.exit(0 if cmd_maintenance(port) else 1)

    elif args.command == "flash":
        sys.exit(0 if cmd_flash(
            port, args.config, args.setools_dir,
            gen_toc=args.gen_toc, maintenance=args.maintenance
        ) else 1)


if __name__ == '__main__':
    main()
