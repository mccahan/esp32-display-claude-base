"""
PlatformIO custom targets and build scripts.
"""

import os
import sys
import time
Import("env")

# Get the project directory
PROJECT_DIR = env.subst("$PROJECT_DIR")


def wait_and_get_status(source, target, env, wait_boot=True):
    """Wait for device to boot and query status via serial."""
    import serial
    import serial.tools.list_ports

    # Find serial port
    upload_port = env.subst("$UPLOAD_PORT")
    if not upload_port:
        # Auto-detect
        for port in serial.tools.list_ports.comports():
            if "usbserial" in port.device.lower():
                upload_port = port.device
                break

    if not upload_port:
        print("Error: Could not find serial port")
        return

    if wait_boot:
        print(f"\nWaiting for device to boot...")
        time.sleep(3)

    print(f"Querying device on {upload_port}...")

    try:
        ser = serial.Serial(upload_port, 115200, timeout=1)
        time.sleep(0.5)
        ser.reset_input_buffer()

        # Send STATUS command
        ser.write(b"STATUS\n")

        # Wait for response
        start_time = time.time()
        buffer = ""
        in_status = False
        status = {}

        while time.time() - start_time < 10:
            if ser.in_waiting:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                buffer += data

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
                            print_status(status)
                            return
                        elif ":" in line:
                            key, value = line.split(":", 1)
                            status[key] = value
            else:
                time.sleep(0.05)

        ser.close()
        print("Timeout waiting for status response")

    except Exception as e:
        print(f"Error: {e}")


def print_status(status):
    """Print formatted status and JSON."""
    import json

    print("\n" + "=" * 40)
    print("Device Status")
    print("=" * 40)

    if status.get("WIFI") == "CONNECTED":
        print("WiFi Status: Connected")
    elif status.get("WIFI") == "AP_MODE":
        print("WiFi Status: AP Mode")
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

    # JSON output for scripting
    json_status = {
        "wifi": status.get("WIFI", "UNKNOWN").lower(),
        "ip": status.get("IP", ""),
        "ssid": status.get("SSID", ""),
        "heap": int(status.get("HEAP", 0)),
        "uptime": int(status.get("UPTIME", 0))
    }
    print(f"\nJSON: {json.dumps(json_status)}\n")


def get_status_no_wait(source, target, env):
    """Get status without waiting for boot."""
    wait_and_get_status(source, target, env, wait_boot=False)


# Add post-upload action to get status after normal upload
env.AddPostAction("upload", wait_and_get_status)

# Register custom target: pio run -t status (just get status, no upload)
env.AddCustomTarget(
    name="status",
    dependencies=None,
    actions=[
        env.VerboseAction(get_status_no_wait, "Getting device status...")
    ],
    title="Device Status",
    description="Query device status via serial"
)
