"""
PlatformIO monitor filter that queries device status and exits.

Automatically sends STATUS command and parses the response.

Usage:
    pio device monitor -f get_status
"""

import os
import sys
import time
from platformio.public import DeviceMonitorFilterBase


class GetStatus(DeviceMonitorFilterBase):
    NAME = "get_status"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._buffer = ""
        self._in_status = False
        self._status_lines = []
        self._command_sent = False
        self._start_time = time.time()

    def rx(self, text):
        """Process received data looking for status response."""
        # Send command on first rx (connection established)
        if not self._command_sent:
            self._command_sent = True
            # Access the serial port through the console's serial attribute
            if hasattr(self, 'console') and hasattr(self.console, 'serial'):
                self.console.serial.write(b"STATUS\n")

        self._buffer += text

        # Check for status markers
        if "---STATUS_BEGIN---" in self._buffer:
            self._in_status = True
            idx = self._buffer.find("---STATUS_BEGIN---")
            self._buffer = self._buffer[idx + len("---STATUS_BEGIN---"):]

        if self._in_status:
            while "\n" in self._buffer:
                line, self._buffer = self._buffer.split("\n", 1)
                line = line.strip()
                if line == "---STATUS_END---":
                    self.print_status()
                    os._exit(0)
                elif line:
                    self._status_lines.append(line)

        # Timeout after 10 seconds
        if time.time() - self._start_time > 10:
            sys.stderr.write("\nTimeout waiting for status response\n")
            os._exit(1)

        return text

    def tx(self, text):
        """Inject STATUS command on first tx opportunity."""
        if not self._command_sent:
            self._command_sent = True
            return "STATUS\n" + text
        return text

    def print_status(self):
        """Print formatted status information."""
        print("\n" + "=" * 40)
        print("Device Status")
        print("=" * 40)

        for line in self._status_lines:
            if ":" in line:
                key, value = line.split(":", 1)
                if key == "WIFI":
                    status_str = value.replace("_", " ").title()
                    print(f"WiFi Status: {status_str}")
                elif key == "IP":
                    print(f"IP Address:  {value}")
                elif key == "SSID":
                    print(f"Network:     {value}")
                elif key == "HEAP":
                    heap_kb = int(value) // 1024
                    print(f"Free Heap:   {heap_kb} KB")
                elif key == "UPTIME":
                    print(f"Uptime:      {value}s")

        print("=" * 40 + "\n")
