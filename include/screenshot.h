#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <Arduino.h>

// Initialize screenshot storage in PSRAM
void initScreenshot();

// Capture screenshot from LVGL framebuffer and save to PSRAM buffer as BMP
bool captureScreenshot();

// Get pointer to screenshot data in PSRAM (BMP format)
const uint8_t* getScreenshotData();

// Get size of screenshot data in bytes
size_t getScreenshotSize();

// Check if screenshot exists in buffer
bool hasScreenshot();

// Mark screenshot as deleted (doesn't free memory, just marks as invalid)
void deleteScreenshot();

#endif // SCREENSHOT_H
