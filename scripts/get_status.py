#!/usr/bin/env python3
"""
Query ESP32 device status via serial port.

Sends STATUS command and parses the response.
Exits after receiving status or on timeout.

Usage:
    python scripts/get_status.py [port]

Examples:
    python scripts/get_status.py                    # Auto-detect port
    python scripts/get_status.py /dev/cu.usbserial-10

Requires pyserial. If not installed system-wide, use PlatformIO's Python:
    $(pio system info --json | jq -r .python_exe) scripts/get_status.py
"""

import sys
import time
import serial
import serial.tools.list_ports


def find_serial_port():
    """Find the ESP32 serial port."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        # Common ESP32 USB serial identifiers
        if "usbserial" in port.device.lower() or "cp210" in port.description.lower():
            return port.device
        if "usb" in port.device.lower() and "serial" in port.description.lower():
            return port.device
    return None


def get_status(port, timeout=10):
    """Query device status and return parsed info."""
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(0.1)  # Let connection stabilize

        # Clear any pending data
        ser.reset_input_buffer()

        # Send STATUS command
        ser.write(b"STATUS\n")

        # Wait for response
        start_time = time.time()
        buffer = ""
        in_status = False
        status_lines = []

        while time.time() - start_time < timeout:
            if ser.in_waiting:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                buffer += data

                # Check for status markers
                if "---STATUS_BEGIN---" in buffer:
                    in_status = True
                    idx = buffer.find("---STATUS_BEGIN---")
                    buffer = buffer[idx + len("---STATUS_BEGIN---"):]

                if in_status:
                    while "\n" in buffer:
                        line, buffer = buffer.split("\n", 1)
                        line = line.strip()
                        if line == "---STATUS_END---":
                            ser.close()
                            return parse_status(status_lines)
                        elif line:
                            status_lines.append(line)
            else:
                time.sleep(0.05)

        ser.close()
        return None

    except serial.SerialException as e:
        print(f"Serial error: {e}", file=sys.stderr)
        return None


def parse_status(lines):
    """Parse status lines into a dict."""
    status = {}
    for line in lines:
        if ":" in line:
            key, value = line.split(":", 1)
            status[key] = value
    return status


def print_status(status):
    """Print formatted status."""
    print("=" * 40)
    print("Device Status")
    print("=" * 40)

    if status.get("WIFI") == "CONNECTED":
        print(f"WiFi Status: Connected")
    elif status.get("WIFI") == "AP_MODE":
        print(f"WiFi Status: AP Mode")
    else:
        print(f"WiFi Status: {status.get('WIFI', 'Unknown')}")

    if "IP" in status:
        print(f"IP Address:  {status['IP']}")

    if "SSID" in status:
        print(f"Network:     {status['SSID']}")

    if "HEAP" in status:
        heap_kb = int(status['HEAP']) // 1024
        print(f"Free Heap:   {heap_kb} KB")

    if "UPTIME" in status:
        print(f"Uptime:      {status['UPTIME']}s")

    print("=" * 40)


def main():
    # Get port from args or auto-detect
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = find_serial_port()
        if not port:
            print("Error: Could not find serial port. Specify port as argument.", file=sys.stderr)
            sys.exit(1)

    print(f"Querying device on {port}...")
    status = get_status(port)

    if status:
        print_status(status)
        # Output just IP for scripting use
        if "IP" in status:
            print(f"\nDevice IP: {status['IP']}")
        sys.exit(0)
    else:
        print("Error: Timeout waiting for status response", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
