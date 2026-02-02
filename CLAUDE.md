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

### System Endpoints
- `GET /api/info` - Device information (heap, PSRAM, uptime, etc.)
- `POST /api/restart` - Restart the device

### WiFi Endpoints
- `GET /api/wifi/status` - Current WiFi status (connected, SSID, RSSI)
- `GET /api/wifi/scan` - Scan for available networks
- `POST /api/wifi/connect` - Save credentials and connect (JSON: `{ssid, password}`)

## Development Workflow

### Building & Flashing
```bash
# Build firmware
pio run

# Flash via USB
pio run --target upload

# Monitor serial output
pio device monitor
```

### OTA Updates (Recommended)
1. Build firmware: `pio run`
2. Navigate to `http://<device-ip>/update`
3. Upload `.pio/build/esp32s3/firmware.bin`

### Taking Screenshots
```bash
# Capture screenshot
curl -X POST http://<device-ip>/api/screenshot/capture

# Download screenshot
curl -o screenshot.bmp http://<device-ip>/api/screenshot/download
```

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

## File Structure

```
├── include/
│   ├── lv_conf.h        # LVGL configuration
│   ├── screenshot.h     # Screenshot API
│   └── web_server.h     # Web server class
├── src/
│   ├── main.cpp         # Main application
│   ├── screenshot.cpp   # BMP screenshot capture
│   └── web_server.cpp   # HTTP endpoints + OTA
├── platformio.ini       # Build configuration
├── partitions.csv       # Flash partitions for OTA
└── sdkconfig.defaults   # ESP-IDF PSRAM config
```

## Iteration with Claude

1. Describe desired UI changes or features
2. I will modify the code and explain changes
3. Build and flash via OTA: `pio run && open http://<ip>/update`
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
