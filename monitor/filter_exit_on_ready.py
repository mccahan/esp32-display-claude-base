"""
PlatformIO monitor filter that exits after detecting "System Ready!" in serial output.

Usage:
    pio device monitor -f exit_on_ready

Or add to platformio.ini:
    monitor_filters = exit_on_ready
"""

import os
import sys
from platformio.public import DeviceMonitorFilterBase


class ExitOnReady(DeviceMonitorFilterBase):
    NAME = "exit_on_ready"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._buffer = ""

    def rx(self, text):
        """Process received data and exit when 'System Ready!' is detected."""
        self._buffer += text

        # Check if we've received the ready message
        if "System Ready!" in self._buffer:
            # Output the text first so user sees the message
            sys.stdout.write(text)
            sys.stdout.flush()
            # Force immediate exit
            os._exit(0)

        # Clear buffer if it gets too long (keep last 100 chars for partial matches)
        if len(self._buffer) > 200:
            self._buffer = self._buffer[-100:]

        return text

    def tx(self, text):
        """Pass through transmitted data unchanged."""
        return text
