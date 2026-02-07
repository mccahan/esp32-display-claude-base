# ESP32 Display Base - Claude-Assisted UI Development

A starter template for rapid UI development on the Guition ESP32-S3-4848S040 display using Claude as your development partner.

> Claude, you are connected to an ESP32 with display over USB. Create an interface that points an arrow towards the nearest aircraft to the user's location. Use open web APIs to get flight data, including flight number, airspeed, altitude, aircraft type, and location. Iterate and confirm the display is functioning by flashing it, pulling a screenshot, and adjusting in a loop until you complete the UI. @DISPLAY-CLAUDE.md

## Screenshots

<p float="left">
  <img src="samples/screenshot-nearest-plane.png" width="200" />
  <img src="samples/screenshot-darkmode.png" width="200" />
  <img src="samples/screenshot-lcars.png" width="200" />
  <img src="samples/screenshot-cyberpunk.png" width="200" />
</p>

## What This Is

This repository provides a working foundation for building custom UIs on a 480x480 touchscreen display. It's designed specifically to enable fast iteration cycles with Claude (or other AI assistants) by including:

- **Screenshot API** - Capture the current display state as a BMP image
- **OTA Updates** - Flash new firmware faster and without USB cables
- **Web Dashboard** - Monitor device status and manage WiFi
- **LVGL Framework** - Industry-standard embedded graphics library

## Hardware

**Guition ESP32-S3-4848S040**
- 480x480 IPS display (ST7701 controller) like [this one](https://www.aliexpress.us/item/3256809197960152.html)
- Capacitive touch (GT911)
- ESP32-S3 with 8MB PSRAM
- 16MB Flash

## Quick Start

### 1. Clone and Configure WiFi

```bash
git clone <this-repo>
cd esp32-display-claude-base
```

Create `include/secrets.h` with your WiFi credentials:
```cpp
#ifndef SECRETS_H
#define SECRETS_H
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
#endif
```

### 2. Build and Flash

```bash
pio run -t upload
```

### 3. Find Your Device

The device will connect to WiFi and display its IP address on screen. You can also check your router or use:
```bash
arp -a | grep -i "esp32"
```

### 4. Start Iterating

Open the web dashboard at `http://<device-ip>` to:
- View device status
- Capture screenshots
- Upload firmware updates

## Development Workflow with Claude

This template enables a conversational development workflow:

1. **Describe** what you want to build or change
2. **Claude modifies** the LVGL UI code in `src/main.cpp`
3. **Build & flash** via OTA: `pio run` then upload at `/update`
4. **Screenshot** the result in the web interface, or:
   ```bash
   curl -X POST http://<ip>/api/screenshot/capture
   curl -o screen.bmp http://<ip>/api/screenshot/download
   ```
5. **Review** the screenshot with Claude and iterate

### Example Prompts

- "Add a battery percentage indicator in the top right corner"
- "Create a settings screen with brightness and WiFi options"
- "Make the status text larger and add a pulsing animation"
- "Design a dashboard showing temperature and humidity"

## Project Structure

```
├── include/
│   ├── lv_conf.h        # LVGL configuration
│   ├── screenshot.h     # Screenshot API
│   ├── web_server.h     # Web server class
│   └── secrets.h        # WiFi credentials (create this, gitignored)
├── lib/
│   └── Arduino_GFX/     # Display driver with ST7701 support
├── src/
│   ├── main.cpp         # Main application & UI code
│   ├── screenshot.cpp   # BMP screenshot capture
│   └── web_server.cpp   # HTTP endpoints + OTA
├── platformio.ini       # Build configuration
├── partitions.csv       # Flash partitions (OTA support)
└── CLAUDE.md            # Detailed technical reference
```

## API Reference

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web dashboard |
| `/update` | GET | OTA firmware upload page |
| `/api/info` | GET | Device info (heap, PSRAM, uptime) |
| `/api/screenshot/capture` | POST | Capture display to memory |
| `/api/screenshot/view` | GET | View screenshot in browser |
| `/api/screenshot/download` | GET | Download screenshot as BMP |
| `/api/wifi/scan` | GET | Scan for WiFi networks |
| `/api/wifi/connect` | POST | Connect to WiFi (JSON body) |
| `/api/restart` | POST | Restart device |

## Customizing the UI

The UI is built with LVGL 8.x. Edit `src/main.cpp`, specifically the `createUI()` function:

```cpp
void createUI() {
    lv_obj_t *scr = lv_scr_act();

    // Set background color
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);

    // Add your widgets here
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hello World");
    lv_obj_center(label);
}
```

See [LVGL Documentation](https://docs.lvgl.io/8.3/) for widget reference.

## Technical Notes

- **Display**: 8MHz pixel clock, full-frame double buffering in PSRAM
- **Touch**: Coordinates transformed from GT911's bottom-right origin
- **Memory**: ~135KB heap, ~6MB PSRAM available after boot
- **Screenshots**: 480x480 24-bit BMP (~675KB)

## Built with this project

- [esp32-display-panel](https://github.com/mccahan/esp32-display-panel) - A home control panel with multiple themes
- [esp32-nearest-plane](https://github.com/mccahan/esp32-nearest-plane) - Show the closest plane to you

## License

MIT

## See Also

- [DISPLAY-CLAUDE.md](DISPLAY-CLAUDE.md) - Detailed technical reference (hardware, APIs, troubleshooting)
- [LVGL Documentation](https://docs.lvgl.io/8.3/)
- [ESP32-S3 Datasheet](https://www.espressif.com/en/products/socs/esp32-s3)
