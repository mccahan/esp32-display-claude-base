#!/bin/bash
# Query ESP32 device status via serial
# Uses PlatformIO's Python (has pyserial bundled)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Find PlatformIO's Python
for p in ~/.platformio/penv/bin/python /usr/local/Cellar/platformio/*/libexec/bin/python; do
    if [ -x "$p" ] 2>/dev/null; then
        exec "$p" "$SCRIPT_DIR/get_status.py" "$@"
    fi
done

echo "Error: PlatformIO Python not found" >&2
exit 1
