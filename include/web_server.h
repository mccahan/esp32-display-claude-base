#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "screenshot.h"

// Maximum JSON payload size for API requests
#define MAX_JSON_PAYLOAD_SIZE 1024

class DisplayWebServer {
public:
    DisplayWebServer();

    // Start the web server
    void begin();

    // Get the server IP address as string
    String getIPAddress();

private:
    AsyncWebServer server;

    void setupRoutes();
    void setupOTA();
    String getIndexPage();
};

extern DisplayWebServer webServer;

#endif // WEB_SERVER_H
