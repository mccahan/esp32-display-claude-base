#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include <Preferences.h>
#include "web_server.h"
#include "screenshot.h"

// ============================================================================
// PIN DEFINITIONS for Guition ESP32-S3-4848S040
// ============================================================================

// Touch controller pins
#define TOUCH_SDA 19
#define TOUCH_SCL 45
#define TOUCH_INT -1  // Not connected
#define TOUCH_RST -1  // Not connected

// Backlight pin
#define GFX_BL 38

// Display dimensions
#define TFT_WIDTH 480
#define TFT_HEIGHT 480

// ============================================================================
// DISPLAY HARDWARE CONFIGURATION
// ============================================================================

// Touch controller instance
TAMC_GT911 touchController(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TFT_WIDTH, TFT_HEIGHT);

// Display bus configuration for ESP32-S3-4848S040
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    39 /* CS */, 48 /* SCK */, 47 /* SDA */,
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    11 /* R0 */, 12 /* R1 */, 13 /* R2 */, 14 /* R3 */, 0 /* R4 */,
    8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
    4 /* B0 */, 5 /* B1 */, 6 /* B2 */, 7 /* B3 */, 15 /* B4 */
);

// ST7701 display panel
Arduino_ST7701_RGBPanel *gfx = new Arduino_ST7701_RGBPanel(
    bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */,
    true /* IPS */, TFT_WIDTH /* width */, TFT_HEIGHT /* height */,
    st7701_type1_init_operations, sizeof(st7701_type1_init_operations),
    true /* BGR */,
    10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */
);

// ============================================================================
// LVGL CONFIGURATION
// ============================================================================

// LVGL display buffers (double buffered in PSRAM)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf1;
static lv_color_t *disp_draw_buf2;
static lv_disp_drv_t disp_drv;

// LVGL tick tracking
static unsigned long last_tick = 0;

// LVGL touch input device
static lv_indev_drv_t indev_drv;
static lv_indev_t *touch_indev = nullptr;

// WiFi preferences storage
Preferences wifi_prefs;

// ============================================================================
// LVGL CALLBACKS
// ============================================================================

// Display flush callback - sends pixels to the display
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    lv_disp_flush_ready(disp);
}

// Touch read callback for LVGL
void my_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    touchController.read();

    if (touchController.isTouched) {
        data->state = LV_INDEV_STATE_PRESSED;

        // Raw touch coordinates from GT911
        int16_t raw_x = touchController.points[0].x;
        int16_t raw_y = touchController.points[0].y;

        // Transform coordinates - GT911 has origin at bottom-right by default
        // Invert both axes for 0° rotation
        data->point.x = TFT_WIDTH - 1 - raw_x;
        data->point.y = TFT_HEIGHT - 1 - raw_y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ============================================================================
// SETUP FUNCTIONS
// ============================================================================

void setupDisplay() {
    Serial.println("Initializing display...");

    // Lower pixel clock (8MHz) reduces tearing
    gfx->begin(8000000);
    gfx->fillScreen(BLACK);

    // Setup backlight PWM
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    Serial.println("Display initialized");
}

void setupLVGL() {
    Serial.println("Initializing LVGL...");

    lv_init();

    // Full frame double buffers in PSRAM for smooth updates
    size_t buf_size = TFT_WIDTH * TFT_HEIGHT;

    disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!disp_draw_buf1 || !disp_draw_buf2) {
        Serial.println("Failed to allocate display buffers in PSRAM!");
        while (1) { delay(1000); }
    }

    Serial.printf("Display buffers allocated: 2 x %u bytes in PSRAM\n", sizeof(lv_color_t) * buf_size);

    // Initialize double buffering
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, disp_draw_buf2, buf_size);

    // Setup display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;  // Always send full frame to reduce tearing
    lv_disp_drv_register(&disp_drv);

    Serial.println("LVGL initialized");
}

void setupTouch() {
    Serial.println("Initializing touch controller...");

    // Initialize I2C for touch controller
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    // Initialize GT911 touch controller
    touchController.begin();
    touchController.setRotation(ROTATION_NORMAL);

    // Register touch input device with LVGL
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    touch_indev = lv_indev_drv_register(&indev_drv);

    Serial.println("Touch controller initialized");
}

void setupWiFi() {
    Serial.println("Setting up WiFi...");

    // Try to load saved credentials
    wifi_prefs.begin("wifi", false);
    String ssid = wifi_prefs.getString("ssid", "");
    String password = wifi_prefs.getString("password", "");
    wifi_prefs.end();

    if (ssid.length() > 0) {
        Serial.printf("Connecting to saved network: %s\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());

        // Wait for connection with timeout
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            return;
        }
    }

    // No saved credentials or connection failed - start AP mode for configuration
    Serial.println("Starting AP mode for WiFi configuration...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-Display", "configure");
    Serial.printf("AP started. Connect to 'ESP32-Display' and visit http://%s\n",
                  WiFi.softAPIP().toString().c_str());
}

// ============================================================================
// UI CREATION
// ============================================================================

// Demo UI with status information
static lv_obj_t *status_label = nullptr;
static lv_obj_t *touch_label = nullptr;

void updateStatusLabel() {
    if (!status_label) return;

    String status = "ESP32 Display Ready\n\n";

    if (WiFi.status() == WL_CONNECTED) {
        status += "WiFi: Connected\n";
        status += "IP: " + WiFi.localIP().toString() + "\n";
        status += "Web: http://" + WiFi.localIP().toString() + "\n";
    } else if (WiFi.getMode() == WIFI_AP) {
        status += "WiFi: AP Mode\n";
        status += "SSID: ESP32-Display\n";
        status += "IP: " + WiFi.softAPIP().toString() + "\n";
    } else {
        status += "WiFi: Disconnected\n";
    }

    status += "\nFree Heap: " + String(ESP.getFreeHeap() / 1024) + " KB\n";
    status += "Free PSRAM: " + String(ESP.getFreePsram() / 1024 / 1024) + " MB\n";
    status += "Uptime: " + String(millis() / 1000) + "s";

    lv_label_set_text(status_label, status.c_str());
}

void createUI() {
    Serial.println("Creating UI...");

    // Get the active screen
    lv_obj_t *scr = lv_scr_act();

    // Set dark background
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);

    // Create title
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ESP32 Display Controller");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00d4ff), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    // Create status label
    status_label = lv_label_create(scr);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xeeeeee), 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 0);
    updateStatusLabel();

    // Create touch feedback label
    touch_label = lv_label_create(scr);
    lv_label_set_text(touch_label, "Touch anywhere to test");
    lv_obj_set_style_text_font(touch_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(touch_label, lv_color_hex(0x888888), 0);
    lv_obj_align(touch_label, LV_ALIGN_BOTTOM_MID, 0, -30);

    // Add touch event to screen
    lv_obj_add_event_cb(scr, [](lv_event_t *e) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_get_act(), &point);

        char buf[64];
        snprintf(buf, sizeof(buf), "Touch: x=%d, y=%d", point.x, point.y);
        lv_label_set_text(touch_label, buf);
    }, LV_EVENT_PRESSED, NULL);

    Serial.println("UI created");
}

// ============================================================================
// ARDUINO SETUP & LOOP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n========================================");
    Serial.println("ESP32 Display Controller Starting...");
    Serial.println("========================================\n");

    // Check PSRAM
    if (psramFound()) {
        Serial.printf("PSRAM found: %d bytes (%d MB)\n",
                      ESP.getPsramSize(), ESP.getPsramSize() / 1024 / 1024);
    } else {
        Serial.println("WARNING: PSRAM not found!");
    }

    // Initialize screenshot storage
    initScreenshot();

    // Setup display hardware
    setupDisplay();

    // Initialize LVGL
    setupLVGL();

    // Initialize touch
    setupTouch();

    // Create the UI
    createUI();

    // Force initial render
    lv_timer_handler();

    // Setup WiFi
    setupWiFi();

    // Start web server
    webServer.begin();

    Serial.println("\n========================================");
    Serial.println("System Ready!");
    Serial.printf("Web interface: http://%s\n",
                  WiFi.status() == WL_CONNECTED ?
                  WiFi.localIP().toString().c_str() :
                  WiFi.softAPIP().toString().c_str());
    Serial.println("OTA updates:   http://<ip>/update");
    Serial.println("Screenshot:    POST /api/screenshot/capture");
    Serial.println("========================================\n");
}

void loop() {
    // Update LVGL tick
    unsigned long now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    // Handle LVGL tasks
    lv_timer_handler();

    // Update status display periodically
    static unsigned long last_status_update = 0;
    if (now - last_status_update > 2000) {
        updateStatusLabel();
        last_status_update = now;
    }

    // Small delay to prevent watchdog issues
    delay(5);
}
