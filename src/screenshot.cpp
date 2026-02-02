#include "screenshot.h"
#include <lvgl.h>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480

// BMP file header structures
#pragma pack(push, 1)
typedef struct {
    uint16_t signature;      // "BM"
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t data_offset;
} BMPFileHeader;

typedef struct {
    uint32_t header_size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_pixels_per_meter;
    int32_t  y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t important_colors;
} BMPInfoHeader;
#pragma pack(pop)

// Global buffer to store screenshot in PSRAM
static uint8_t* screenshot_buffer = nullptr;
static size_t screenshot_size = 0;
static bool screenshot_available = false;

void initScreenshot() {
    // Calculate required buffer size for BMP
    uint32_t row_size = ((SCREEN_WIDTH * 3 + 3) / 4) * 4;  // Row must be multiple of 4 bytes
    uint32_t image_size = row_size * SCREEN_HEIGHT;
    size_t total_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + image_size;

    // Allocate buffer in PSRAM
    screenshot_buffer = (uint8_t*)heap_caps_malloc(total_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (screenshot_buffer) {
        Serial.printf("Screenshot buffer allocated: %u bytes in PSRAM\n", total_size);
    } else {
        Serial.println("Failed to allocate screenshot buffer in PSRAM");
    }
}

// Convert RGB565 to RGB888 (24-bit BMP)
static void rgb565_to_rgb888(uint16_t rgb565, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = ((rgb565 >> 11) & 0x1F) << 3;  // 5 bits red
    *g = ((rgb565 >> 5) & 0x3F) << 2;   // 6 bits green
    *b = (rgb565 & 0x1F) << 3;          // 5 bits blue
}

bool captureScreenshot() {
    Serial.println("Capturing screenshot...");

    if (!screenshot_buffer) {
        Serial.println("Screenshot buffer not initialized");
        return false;
    }

    // Get LVGL display and screen buffer
    lv_disp_t* disp = lv_disp_get_default();
    if (!disp) {
        Serial.println("No display found");
        return false;
    }

    // Get the active draw buffer (contains current display state)
    lv_disp_draw_buf_t* draw_buf = lv_disp_get_draw_buf(disp);
    if (!draw_buf || !draw_buf->buf_act) {
        Serial.println("No active buffer");
        return false;
    }

    // Validate buffer size
    uint32_t expected_size = SCREEN_WIDTH * SCREEN_HEIGHT;
    if (draw_buf->size < expected_size) {
        Serial.printf("Buffer too small: %u < %u\n", draw_buf->size, expected_size);
        return false;
    }

    lv_color_t* buf = (lv_color_t*)draw_buf->buf_act;

    // Calculate sizes
    uint32_t row_size = ((SCREEN_WIDTH * 3 + 3) / 4) * 4;  // BMP rows must be multiple of 4 bytes
    uint32_t image_size = row_size * SCREEN_HEIGHT;
    uint32_t file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + image_size;

    screenshot_size = file_size;
    uint8_t* write_ptr = screenshot_buffer;

    // Write BMP file header
    BMPFileHeader file_header = {0};
    file_header.signature = 0x4D42;  // "BM"
    file_header.file_size = file_size;
    file_header.data_offset = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
    memcpy(write_ptr, &file_header, sizeof(BMPFileHeader));
    write_ptr += sizeof(BMPFileHeader);

    // Write BMP info header
    BMPInfoHeader info_header = {0};
    info_header.header_size = sizeof(BMPInfoHeader);
    info_header.width = SCREEN_WIDTH;
    info_header.height = SCREEN_HEIGHT;  // Positive = bottom-up
    info_header.planes = 1;
    info_header.bits_per_pixel = 24;
    info_header.compression = 0;  // No compression
    info_header.image_size = image_size;
    memcpy(write_ptr, &info_header, sizeof(BMPInfoHeader));
    write_ptr += sizeof(BMPInfoHeader);

    // Write pixel data (BMP is bottom-up, so write rows in reverse)
    for (int y = SCREEN_HEIGHT - 1; y >= 0; y--) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int pixel_index = y * SCREEN_WIDTH + x;
            uint16_t rgb565 = buf[pixel_index].full;

            uint8_t r, g, b;
            rgb565_to_rgb888(rgb565, &r, &g, &b);

            // BMP uses BGR order
            *write_ptr++ = b;
            *write_ptr++ = g;
            *write_ptr++ = r;
        }

        // Add padding to make row size multiple of 4
        int padding = row_size - (SCREEN_WIDTH * 3);
        for (int p = 0; p < padding; p++) {
            *write_ptr++ = 0;
        }
    }

    screenshot_available = true;
    Serial.printf("Screenshot captured: %u bytes\n", file_size);
    return true;
}

const uint8_t* getScreenshotData() {
    return screenshot_buffer;
}

size_t getScreenshotSize() {
    return screenshot_size;
}

bool hasScreenshot() {
    return screenshot_available && screenshot_buffer != nullptr;
}

void deleteScreenshot() {
    screenshot_available = false;
    screenshot_size = 0;
}
