#include "web_server.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ElegantOTA.h>
#include <Preferences.h>

// External variables for touch simulation (defined in main.cpp)
extern volatile bool simulated_touch_active;
extern volatile int16_t simulated_touch_x;
extern volatile int16_t simulated_touch_y;
extern volatile unsigned long simulated_touch_start;

// Global instance
DisplayWebServer webServer;

DisplayWebServer::DisplayWebServer() : server(80) {
}

void DisplayWebServer::begin() {
    setupRoutes();
    setupOTA();
    server.begin();
    Serial.println("Web server started on port 80");
}

String DisplayWebServer::getIPAddress() {
    return WiFi.localIP().toString();
}

void DisplayWebServer::setupOTA() {
    // ElegantOTA provides a nice web UI for firmware updates
    ElegantOTA.begin(&server);

    // Log OTA start
    ElegantOTA.onStart([]() {
        Serial.println("OTA update started...");
    });

    // Log OTA progress
    ElegantOTA.onProgress([](size_t current, size_t total) {
        static int lastPercent = -1;
        int percent = (current * 100) / total;
        if (percent != lastPercent && percent % 10 == 0) {
            Serial.printf("OTA progress: %d%%\n", percent);
            lastPercent = percent;
        }
    });

    // Reset device when OTA completes successfully
    ElegantOTA.onEnd([this](bool success) {
        if (success) {
            Serial.println("OTA update successful, restarting...");
            server.end(); // Explicitly close open HTTP connections
        } else {
            Serial.println("OTA update failed");
        }
    });

    Serial.println("OTA updates available at /update");
}

void DisplayWebServer::setupRoutes() {
    // Root page - simple dashboard
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", getIndexPage());
    });

    // API: Get device info
    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<512> doc;
        doc["chip_model"] = ESP.getChipModel();
        doc["chip_revision"] = ESP.getChipRevision();
        doc["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
        doc["flash_size"] = ESP.getFlashChipSize();
        doc["free_heap"] = ESP.getFreeHeap();
        doc["free_psram"] = ESP.getFreePsram();
        doc["total_psram"] = ESP.getPsramSize();
        doc["uptime_seconds"] = millis() / 1000;
        doc["ip_address"] = WiFi.localIP().toString();
        doc["mac_address"] = WiFi.macAddress();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API: Capture screenshot
    server.on("/api/screenshot/capture", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool success = captureScreenshot();

        StaticJsonDocument<128> doc;
        doc["success"] = success;
        if (success) {
            doc["size"] = getScreenshotSize();
            doc["message"] = "Screenshot captured";
        } else {
            doc["message"] = "Failed to capture screenshot";
        }

        String response;
        serializeJson(doc, response);
        request->send(success ? 200 : 500, "application/json", response);
    });

    // API: Download screenshot
    server.on("/api/screenshot/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!hasScreenshot()) {
            request->send(404, "application/json", "{\"error\":\"No screenshot available\"}");
            return;
        }

        const uint8_t* data = getScreenshotData();
        size_t size = getScreenshotSize();

        // Send as downloadable BMP file
        AsyncWebServerResponse *response = request->beginResponse(
            200, "image/bmp", data, size
        );
        response->addHeader("Content-Disposition", "attachment; filename=\"screenshot.bmp\"");
        request->send(response);
    });

    // API: View screenshot in browser
    server.on("/api/screenshot/view", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!hasScreenshot()) {
            request->send(404, "application/json", "{\"error\":\"No screenshot available\"}");
            return;
        }

        const uint8_t* data = getScreenshotData();
        size_t size = getScreenshotSize();

        // Send as inline image (viewable in browser)
        AsyncWebServerResponse *response = request->beginResponse(
            200, "image/bmp", data, size
        );
        response->addHeader("Content-Disposition", "inline; filename=\"screenshot.bmp\"");
        request->send(response);
    });

    // API: Screenshot status
    server.on("/api/screenshot/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<128> doc;
        doc["available"] = hasScreenshot();
        if (hasScreenshot()) {
            doc["size"] = getScreenshotSize();
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API: Delete screenshot
    server.on("/api/screenshot/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
        deleteScreenshot();

        StaticJsonDocument<64> doc;
        doc["success"] = true;
        doc["message"] = "Screenshot deleted";

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API: Restart device
    server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"message\":\"Restarting...\"}");
        delay(100);
        ESP.restart();
    });

    // API: Simulate touch at a point (GET with query params: ?x=240&y=240)
    server.on("/api/touch/simulate", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Check for required x and y parameters
        if (!request->hasParam("x") || !request->hasParam("y")) {
            request->send(400, "text/plain", "x and y required");
            return;
        }

        int x = request->getParam("x")->value().toInt();
        int y = request->getParam("y")->value().toInt();

        // Validate coordinates are within display bounds
        if (x < 0 || x >= 480 || y < 0 || y >= 480) {
            request->send(400, "text/plain", "Out of bounds (0-479)");
            return;
        }

        // Set simulated touch state
        simulated_touch_x = (int16_t)x;
        simulated_touch_y = (int16_t)y;
        simulated_touch_start = millis();
        simulated_touch_active = true;

        Serial.printf("Simulating touch at (%d, %d)\n", x, y);

        char buf[64];
        snprintf(buf, sizeof(buf), "{\"success\":true,\"x\":%d,\"y\":%d}", x, y);
        request->send(200, "application/json", buf);
    });

    // API: Get WiFi status
    server.on("/api/wifi/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;
        doc["connected"] = (WiFi.status() == WL_CONNECTED);
        doc["mode"] = (WiFi.getMode() == WIFI_AP) ? "ap" : "station";
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();

        if (WiFi.getMode() == WIFI_AP) {
            doc["ap_ip"] = WiFi.softAPIP().toString();
            doc["ap_ssid"] = "ESP32-Display";
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API: Scan WiFi networks
    server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        int n = WiFi.scanNetworks();
        StaticJsonDocument<2048> doc;
        JsonArray networks = doc.createNestedArray("networks");

        for (int i = 0; i < n && i < 20; i++) {
            JsonObject net = networks.createNestedObject();
            net["ssid"] = WiFi.SSID(i);
            net["rssi"] = WiFi.RSSI(i);
            net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }

        WiFi.scanDelete();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API: Connect to WiFi (with body handler for POST data)
    server.on("/api/wifi/connect", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            // Response is sent after body is processed
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            const char* ssid = doc["ssid"];
            const char* password = doc["password"] | "";

            if (!ssid || strlen(ssid) == 0) {
                request->send(400, "application/json", "{\"error\":\"SSID required\"}");
                return;
            }

            // Save credentials
            Preferences prefs;
            prefs.begin("wifi", false);
            prefs.putString("ssid", ssid);
            prefs.putString("password", password);
            prefs.end();

            StaticJsonDocument<128> response;
            response["success"] = true;
            response["message"] = "WiFi credentials saved. Restarting...";

            String responseStr;
            serializeJson(response, responseStr);
            request->send(200, "application/json", responseStr);

            // Restart to apply new WiFi settings
            delay(500);
            ESP.restart();
        }
    );
}

String DisplayWebServer::getIndexPage() {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Display Controller</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1a1a2e;
            color: #eee;
            min-height: 100vh;
            padding: 20px;
        }
        .container { max-width: 800px; margin: 0 auto; }
        h1 { color: #00d4ff; margin-bottom: 20px; }
        .card {
            background: #16213e;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 20px;
            border: 1px solid #0f3460;
        }
        .card h2 { color: #00d4ff; margin-bottom: 15px; font-size: 1.2em; }
        .info-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 15px; }
        .info-item { background: #0f3460; padding: 12px; border-radius: 8px; }
        .info-label { color: #888; font-size: 0.85em; margin-bottom: 4px; }
        .info-value { font-size: 1.1em; font-weight: 500; }
        .btn {
            background: #00d4ff;
            color: #1a1a2e;
            border: none;
            padding: 12px 24px;
            border-radius: 8px;
            font-size: 1em;
            cursor: pointer;
            margin-right: 10px;
            margin-bottom: 10px;
            transition: background 0.2s;
        }
        .btn:hover { background: #00b8e6; }
        .btn-secondary { background: #0f3460; color: #eee; }
        .btn-secondary:hover { background: #1a4a7a; }
        .btn-danger { background: #e94560; }
        .btn-danger:hover { background: #d13550; }
        .screenshot-container { text-align: center; margin-top: 15px; }
        .screenshot-container img {
            max-width: 100%;
            border-radius: 8px;
            border: 2px solid #0f3460;
        }
        .status { padding: 8px 16px; border-radius: 4px; display: inline-block; margin-top: 10px; }
        .status-success { background: #0f5132; color: #75b798; }
        .status-error { background: #5c1a1a; color: #ea868f; }
        #screenshot-status { margin-bottom: 15px; }
        .form-group { margin-bottom: 15px; }
        .form-group label { display: block; color: #888; margin-bottom: 5px; }
        .form-group input, .form-group select {
            width: 100%;
            padding: 10px;
            border: 1px solid #0f3460;
            border-radius: 6px;
            background: #0f3460;
            color: #eee;
            font-size: 1em;
        }
        .form-group input:focus, .form-group select:focus {
            outline: none;
            border-color: #00d4ff;
        }
        .network-list { max-height: 200px; overflow-y: auto; margin-bottom: 15px; }
        .network-item {
            padding: 10px;
            background: #0f3460;
            border-radius: 6px;
            margin-bottom: 8px;
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .network-item:hover { background: #1a4a7a; }
        .signal { font-size: 0.9em; color: #888; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Display Controller</h1>

        <div class="card">
            <h2>Device Information</h2>
            <div class="info-grid" id="device-info">
                <div class="info-item">
                    <div class="info-label">Loading...</div>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>Screenshot</h2>
            <div id="screenshot-status"></div>
            <button class="btn" onclick="captureScreenshot()">Capture Screenshot</button>
            <button class="btn btn-secondary" onclick="viewScreenshot()">View</button>
            <button class="btn btn-secondary" onclick="downloadScreenshot()">Download</button>
            <div class="screenshot-container" id="screenshot-container"></div>
        </div>

        <div class="card">
            <h2>WiFi Configuration</h2>
            <div id="wifi-status" style="margin-bottom: 15px;"></div>
            <button class="btn btn-secondary" onclick="scanNetworks()">Scan Networks</button>
            <div id="network-list" class="network-list" style="display:none;"></div>
            <div class="form-group">
                <label>SSID</label>
                <input type="text" id="wifi-ssid" placeholder="Network name">
            </div>
            <div class="form-group">
                <label>Password</label>
                <input type="password" id="wifi-password" placeholder="Password (leave empty for open networks)">
            </div>
            <button class="btn" onclick="connectWifi()">Save & Connect</button>
        </div>

        <div class="card">
            <h2>Firmware Update</h2>
            <p style="margin-bottom: 15px; color: #888;">
                Upload new firmware via the OTA update interface.
            </p>
            <a href="/update" class="btn">Open OTA Update</a>
        </div>

        <div class="card">
            <h2>System</h2>
            <button class="btn btn-danger" onclick="restartDevice()">Restart Device</button>
        </div>
    </div>

    <script>
        async function loadDeviceInfo() {
            try {
                const response = await fetch('/api/info');
                const data = await response.json();

                const grid = document.getElementById('device-info');
                grid.innerHTML = `
                    <div class="info-item">
                        <div class="info-label">Chip</div>
                        <div class="info-value">${data.chip_model} Rev ${data.chip_revision}</div>
                    </div>
                    <div class="info-item">
                        <div class="info-label">CPU Frequency</div>
                        <div class="info-value">${data.cpu_freq_mhz} MHz</div>
                    </div>
                    <div class="info-item">
                        <div class="info-label">Free Heap</div>
                        <div class="info-value">${(data.free_heap / 1024).toFixed(1)} KB</div>
                    </div>
                    <div class="info-item">
                        <div class="info-label">PSRAM</div>
                        <div class="info-value">${(data.free_psram / 1024 / 1024).toFixed(1)} / ${(data.total_psram / 1024 / 1024).toFixed(1)} MB</div>
                    </div>
                    <div class="info-item">
                        <div class="info-label">Uptime</div>
                        <div class="info-value">${formatUptime(data.uptime_seconds)}</div>
                    </div>
                    <div class="info-item">
                        <div class="info-label">IP Address</div>
                        <div class="info-value">${data.ip_address}</div>
                    </div>
                `;
            } catch (e) {
                console.error('Failed to load device info:', e);
            }
        }

        function formatUptime(seconds) {
            const h = Math.floor(seconds / 3600);
            const m = Math.floor((seconds % 3600) / 60);
            const s = seconds % 60;
            return `${h}h ${m}m ${s}s`;
        }

        async function captureScreenshot() {
            try {
                const response = await fetch('/api/screenshot/capture', { method: 'POST' });
                const data = await response.json();

                const status = document.getElementById('screenshot-status');
                if (data.success) {
                    status.innerHTML = `<span class="status status-success">Screenshot captured (${(data.size / 1024).toFixed(1)} KB)</span>`;
                    viewScreenshot();
                } else {
                    status.innerHTML = `<span class="status status-error">${data.message}</span>`;
                }
            } catch (e) {
                console.error('Failed to capture screenshot:', e);
            }
        }

        function viewScreenshot() {
            const container = document.getElementById('screenshot-container');
            container.innerHTML = `<img src="/api/screenshot/view?t=${Date.now()}" alt="Screenshot" onerror="this.parentElement.innerHTML='<p style=\\'color:#888\\'>No screenshot available</p>'">`;
        }

        function downloadScreenshot() {
            window.location.href = '/api/screenshot/download';
        }

        async function restartDevice() {
            if (confirm('Are you sure you want to restart the device?')) {
                await fetch('/api/restart', { method: 'POST' });
                alert('Device is restarting...');
            }
        }

        async function loadWifiStatus() {
            try {
                const response = await fetch('/api/wifi/status');
                const data = await response.json();

                const status = document.getElementById('wifi-status');
                if (data.connected) {
                    status.innerHTML = `<span class="status status-success">Connected to ${data.ssid} (${data.rssi} dBm)</span>`;
                } else if (data.mode === 'ap') {
                    status.innerHTML = `<span class="status status-error">AP Mode: Connect to "${data.ap_ssid}" to configure</span>`;
                } else {
                    status.innerHTML = `<span class="status status-error">Disconnected</span>`;
                }
            } catch (e) {
                console.error('Failed to load WiFi status:', e);
            }
        }

        async function scanNetworks() {
            const list = document.getElementById('network-list');
            list.style.display = 'block';
            list.innerHTML = '<div style="padding: 10px; color: #888;">Scanning...</div>';

            try {
                const response = await fetch('/api/wifi/scan');
                const data = await response.json();

                if (data.networks.length === 0) {
                    list.innerHTML = '<div style="padding: 10px; color: #888;">No networks found</div>';
                    return;
                }

                list.innerHTML = data.networks.map(net =>
                    `<div class="network-item" onclick="selectNetwork('${net.ssid}')">
                        <span>${net.ssid} ${net.secure ? '🔒' : ''}</span>
                        <span class="signal">${net.rssi} dBm</span>
                    </div>`
                ).join('');
            } catch (e) {
                list.innerHTML = '<div style="padding: 10px; color: #ea868f;">Scan failed</div>';
            }
        }

        function selectNetwork(ssid) {
            document.getElementById('wifi-ssid').value = ssid;
            document.getElementById('network-list').style.display = 'none';
            document.getElementById('wifi-password').focus();
        }

        async function connectWifi() {
            const ssid = document.getElementById('wifi-ssid').value;
            const password = document.getElementById('wifi-password').value;

            if (!ssid) {
                alert('Please enter an SSID');
                return;
            }

            try {
                const response = await fetch('/api/wifi/connect', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ ssid, password })
                });
                const data = await response.json();

                if (data.success) {
                    alert('WiFi credentials saved. Device will restart and connect to the new network.');
                } else {
                    alert('Error: ' + data.error);
                }
            } catch (e) {
                console.error('Failed to save WiFi:', e);
            }
        }

        // Load data on page load
        loadDeviceInfo();
        loadWifiStatus();
        setInterval(loadDeviceInfo, 5000);
        setInterval(loadWifiStatus, 10000);

        // Check for existing screenshot
        fetch('/api/screenshot/status')
            .then(r => r.json())
            .then(data => {
                if (data.available) {
                    document.getElementById('screenshot-status').innerHTML =
                        `<span class="status status-success">Screenshot available (${(data.size / 1024).toFixed(1)} KB)</span>`;
                    viewScreenshot();
                }
            });
    </script>
</body>
</html>
)rawliteral";
}
