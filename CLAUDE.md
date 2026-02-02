# Claude Agent Instructions

This is an ESP32 display project for the Guition ESP32-S3-4848S040 (480x480 touchscreen).

## Key Files

- **DISPLAY-CLAUDE.md** - Detailed hardware specs, pin mappings, API reference, and troubleshooting
- **src/main.cpp** - Main application code including `createUI()` function for LVGL widgets
- **include/secrets.h** - WiFi credentials (gitignored, must be created)

## Common Tasks

### Modify the UI
Edit `createUI()` in `src/main.cpp`. Uses LVGL 8.x widgets.

### Build & Flash
```bash
pio run -t upload
```

### Take a Screenshot
```bash
curl -X POST http://<device-ip>/api/screenshot/capture
curl -o screenshot.bmp http://<device-ip>/api/screenshot/download
```

### OTA Update
Upload `.pio/build/esp32s3/firmware.bin` at `http://<device-ip>/update`

## Before Making Changes

Read **DISPLAY-CLAUDE.md** for:
- Hardware pin configuration
- Web API endpoints
- LVGL configuration details
- Common issues and solutions
