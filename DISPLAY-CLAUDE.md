# ESP32 Display Controller - Claude Development Guide

## Project Overview

Base firmware for the Guition ESP32-S3-4848S040 development board featuring:
- 480x480 IPS display with ST7701 controller
- GT911 capacitive touch controller
- 16MB flash with OTA partition support
- Octal PSRAM for display buffers

## Hardware Configuration

### Board: Guition ESP32-S3-4848S040
- **Display**: 480x480 RGB IPS panel via ST7701 driver
- **Touch**: GT911 capacitive controller on I2C
- **Memory**: 16MB Flash (DIO), 8MB PSRAM (OPI)
- **Backlight**: GPIO 38 (PWM capable)

### Key Pin Mappings
| Function | GPIO |
|----------|------|
| Touch SDA | 19 |
| Touch SCL | 45 |
| Backlight | 38 |
| PCLK | 21 |
| DE | 18 |
| VSYNC | 17 |
| HSYNC | 16 |

## Web Server API

### Screenshot Endpoints
- `POST /api/screenshot/capture` - Capture current display to BMP
- `GET /api/screenshot/view` - View screenshot in browser
- `GET /api/screenshot/download` - Download as file
- `GET /api/screenshot/status` - Check if screenshot exists

### OTA Updates
- Navigate to `http://<device-ip>/update` for firmware upload
- Uses ElegantOTA library with web UI
- Device automatically restarts after successful update

### Touch Simulation
- `GET /api/touch/simulate?x=240&y=240` - Simulate a touch at specified coordinates
  - Query params: `x` and `y` (coordinates 0-479)
  - Touch lasts ~150ms (enough to trigger LVGL events)

### System Endpoints
- `GET /api/info` - Device information (heap, PSRAM, uptime, etc.)
- `POST /api/restart` - Restart the device

### WiFi Endpoints
- `GET /api/wifi/status` - Current WiFi status (connected, SSID, RSSI)
- `GET /api/wifi/scan` - Scan for available networks
- `POST /api/wifi/connect` - Save credentials and connect (JSON: `{ssid, password}`)

## Development Workflow

### Building & Flashing via USB
```bash
# Build and flash via USB - automatically shows IP/WiFi status after flash
pio run -t upload
```

Output after flash:
```
========================================
Device Status
========================================
WiFi Status: Connected
IP Address:  10.0.1.46
Network:     Your Network
Free Heap:   135 KB
Uptime:      6s
========================================
```

### Getting Device Status (without flashing)
```bash
# Query device status via serial
pio run -t status

# Or use the shell script
./scripts/status.sh
```

### Other Commands
```bash
# Build only (no flash)
pio run

# Monitor serial output
pio device monitor
```

### OTA Updates (Recommended for iteration)
```bash
# Automated: build, upload, wait for reboot
./scripts/deploy.sh <device-ip>

# Or manually:
# 1. Build firmware
pio run
# 2. Navigate to http://<device-ip>/update
# 3. Upload .pio/build/esp32s3/firmware.bin
# Device auto-restarts after successful upload
```

### Taking Screenshots
```bash
# Capture screenshot
curl -X POST http://<device-ip>/api/screenshot/capture

# Download screenshot
curl -o screenshot.bmp http://<device-ip>/api/screenshot/download
```

### Simulating Touch
```bash
# Simulate touch at center of screen
curl "http://<device-ip>/api/touch/simulate?x=240&y=240"

# Simulate touch at specific button location
curl "http://<device-ip>/api/touch/simulate?x=100&y=400"
```

## Serial Commands

The device responds to commands sent via serial (115200 baud):

| Command | Response |
|---------|----------|
| `STATUS` | Device status (WiFi, IP, heap, uptime) |

Example response:
```
---STATUS_BEGIN---
WIFI:CONNECTED
IP:10.0.1.46
SSID:YourNetwork
HEAP:138240
UPTIME:120
---STATUS_END---
```

Use `./scripts/status.sh` for a formatted view, or send commands directly via `pio device monitor`.

## WiFi Configuration

On first boot or if WiFi connection fails:
1. Device creates AP: "ESP32-Display" (password: "configure")
2. Connect to the AP with your phone/computer
3. Navigate to `http://192.168.4.1`
4. Use the WiFi Configuration section to scan and select a network
5. Enter password and click "Save & Connect"
6. Device restarts and connects to the configured network

To reconfigure WiFi later, access the web interface at the device's IP address.

Saved credentials are stored in NVS under namespace "wifi".

## LVGL Notes

- Using LVGL 8.3.11
- Double-buffered full-frame rendering in PSRAM
- 16-bit color depth (RGB565)
- Full refresh mode to reduce tearing

### Layout Tips
- Screen is 480x480 pixels
- Center calculation: `start_x = (480 - total_width) / 2`
- Leave ~25px margin from edges for comfortable touch targets
- Button height of 45-55px works well for touch
- Card gaps of 15-20px provide good visual separation

## File Structure

```
├── include/
│   ├── lv_conf.h        # LVGL configuration
│   ├── screenshot.h     # Screenshot API
│   ├── web_server.h     # Web server class
│   └── secrets.h        # WiFi credentials (gitignored)
├── lib/
│   └── Arduino_GFX/     # Display library with ST7701 support
├── src/
│   ├── main.cpp         # Main application
│   ├── screenshot.cpp   # BMP screenshot capture
│   └── web_server.cpp   # HTTP endpoints + OTA
├── scripts/
│   ├── status.sh        # Query device IP/status via serial
│   ├── deploy.sh        # OTA deploy: build, upload, verify
│   └── get_status.py    # Serial status query (used by status.sh)
├── monitor/
│   ├── filter_exit_on_ready.py  # Exit monitor after "System Ready!"
│   └── filter_get_status.py     # Exit monitor after status response
├── extra_script.py      # PlatformIO custom targets (upload, status)
├── platformio.ini       # Build configuration
├── partitions.csv       # Flash partitions for OTA
└── sdkconfig.defaults   # ESP-IDF PSRAM config
```

### secrets.h (gitignored)
Create `include/secrets.h` with your WiFi credentials:
```cpp
#ifndef SECRETS_H
#define SECRETS_H
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
#endif
```

## Iteration with Claude

1. Describe desired UI changes or features
2. I will modify the code and explain changes
3. Deploy: `./scripts/deploy.sh <ip>` (or get IP first with `./scripts/status.sh`)
4. Capture screenshot: `curl -X POST http://<ip>/api/screenshot/capture`
5. Download and review: `curl -o screen.bmp http://<ip>/api/screenshot/download`
6. Repeat as needed

## Common Issues

### Display is blank
- Check backlight GPIO 38 is HIGH
- Verify PSRAM is detected (check serial output)
- Ensure pixel clock is 8MHz (higher can cause issues)

### Touch coordinates inverted
- GT911 origin is bottom-right by default
- Coordinates are transformed in `my_touchpad_read()`

### OTA upload fails
- Ensure partition table supports OTA (app0/app1)
- Check available space with `/api/info`
- OTA via curl may not work reliably; use USB flash: `pio run -t upload`

### Screenshot shows wrong/old content
- With double buffering, `buf_act` points to the NEXT frame being prepared
- Screenshot code should read from the OTHER buffer (currently displayed)
- The fix reads `buf1` if `buf_act == buf2` and vice versa
- `lv_snapshot_take()` may crash on ESP32 due to memory constraints

### USB Flash is slow
- Normal speed is ~128 kbit/s, takes ~70 seconds for ~1MB firmware
- This is expected behavior for this board
