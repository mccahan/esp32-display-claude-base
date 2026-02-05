#!/bin/bash

#
# OTA Deploy Script for ESP32 Display
#
# Usage: ./scripts/deploy.sh [device-ip]
#
# Examples:
#   ./scripts/deploy.sh 10.0.1.66
#   ./scripts/deploy.sh                    # Uses default IP
#   DEVICE_IP=10.0.1.66 ./scripts/deploy.sh
#

set -e

FIRMWARE_PATH=".pio/build/esp32s3/firmware.bin"
DEFAULT_DEVICE_IP="${DEVICE_IP:-10.0.1.66}"
DEVICE_IP="${1:-$DEFAULT_DEVICE_IP}"
BASE_URL="http://${DEVICE_IP}"
REBOOT_TIMEOUT=30

echo ""
echo "🚀 ESP32 Display OTA Deploy"
echo "   Target: ${DEVICE_IP}"
echo ""

# Step 1: Build firmware
echo "📦 Building firmware..."
pio run

# Step 2: Verify firmware exists
if [ ! -f "$FIRMWARE_PATH" ]; then
    echo "❌ Firmware not found: $FIRMWARE_PATH"
    exit 1
fi

FIRMWARE_SIZE=$(stat -f%z "$FIRMWARE_PATH" 2>/dev/null || stat -c%s "$FIRMWARE_PATH")
FIRMWARE_SIZE_KB=$((FIRMWARE_SIZE / 1024))
MD5_HASH=$(md5 -q "$FIRMWARE_PATH" 2>/dev/null || md5sum "$FIRMWARE_PATH" | cut -d' ' -f1)

echo ""
echo "📄 Firmware: $FIRMWARE_PATH"
echo "   Size: ${FIRMWARE_SIZE_KB} KB"
echo "   MD5:  ${MD5_HASH}"
echo ""

# Step 3: Check device is reachable
echo "🔍 Checking device connectivity..."
if ! curl -sf --connect-timeout 5 "${BASE_URL}/api/info" > /tmp/device_info.json 2>/dev/null; then
    echo "❌ Cannot reach device at ${DEVICE_IP}"
    exit 1
fi

CHIP_MODEL=$(cat /tmp/device_info.json | grep -o '"chip_model":"[^"]*"' | cut -d'"' -f4)
UPTIME=$(cat /tmp/device_info.json | grep -o '"uptime_seconds":[0-9]*' | cut -d':' -f2)
echo "   Device: ${CHIP_MODEL}"
echo "   Uptime: ${UPTIME}s"
echo ""

# Step 4: Start OTA update via ElegantOTA
echo "📤 Starting OTA update..."

# ElegantOTA v3 uses /ota/start and /ota/upload endpoints
# mode=fr means firmware (vs fs for filesystem)
START_RESULT=$(curl -sf --connect-timeout 5 "${BASE_URL}/ota/start?mode=fr&hash=${MD5_HASH}" 2>&1)
if [ $? -ne 0 ]; then
    echo "   ⚠️  Failed to start OTA, trying anyway..."
fi

echo "   Uploading firmware..."

# Upload the firmware file
UPLOAD_RESULT=$(curl -sf --connect-timeout 10 --max-time 120 \
    -F "firmware=@${FIRMWARE_PATH}" \
    "${BASE_URL}/ota/upload" 2>&1) || true

# Device will reboot, so connection may be reset - that's expected
echo "   Upload complete, device should be rebooting..."

# Step 5: Wait for device to go offline (reboot)
echo ""
echo "🔄 Waiting for device to reboot..."

OFFLINE_DETECTED=false
for i in $(seq 1 10); do
    sleep 1
    if ! curl -sf --connect-timeout 1 "${BASE_URL}/api/info" > /dev/null 2>&1; then
        OFFLINE_DETECTED=true
        echo "   Device went offline (rebooting)"
        break
    fi
    echo -n "."
done

if [ "$OFFLINE_DETECTED" = false ]; then
    echo ""
    echo "⚠️  Device may not have rebooted, checking status..."
fi

# Step 6: Wait for device to come back online
echo ""
echo "⏳ Waiting for device to come back online..."

ONLINE=false
for i in $(seq 1 $REBOOT_TIMEOUT); do
    sleep 1
    if curl -sf --connect-timeout 2 "${BASE_URL}/api/info" > /tmp/device_info.json 2>/dev/null; then
        ONLINE=true
        break
    fi
    echo -n "."
done

echo ""

if [ "$ONLINE" = true ]; then
    NEW_UPTIME=$(cat /tmp/device_info.json | grep -o '"uptime_seconds":[0-9]*' | cut -d':' -f2)
    echo ""
    echo "✅ OTA update successful!"
    echo "   Device uptime: ${NEW_UPTIME}s (freshly rebooted)"
    echo "   Web UI: ${BASE_URL}"
    echo ""
else
    echo ""
    echo "❌ Device did not come back online within ${REBOOT_TIMEOUT}s"
    exit 1
fi

# Cleanup
rm -f /tmp/device_info.json
