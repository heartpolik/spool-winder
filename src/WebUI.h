#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "Winder.h"

class WebUI {
public:
    void begin(Winder& winder);
    void update(); // call every loop() iteration - times out stale remote jog
    String ip() const { return ip_; }

    // True if some client (browser, Cardputer remote, etc) has hit the API
    // recently - shown as a small icon on the OLED. Based on request
    // recency, not WiFi.softAPgetStationNum(): that count is known to stick
    // on ESP32 when a station disappears without a clean disconnect (out of
    // range, app killed, etc), so it doesn't reliably go back to 0.
    bool hasClient() const;

private:
    void setupRoutes();
    void noteActivity() { lastApiActivityMs_ = millis(); }

    AsyncWebServer server_{80};
    Winder* winder_ = nullptr;
    String ip_;

    unsigned long lastApiActivityMs_ = 0;

    // A remote client must keep POSTing /api/jog/start while it wants the
    // jog held (heartbeat); if it goes quiet (dropped connection, closed
    // app) for JOG_REMOTE_TIMEOUT_MS, jog is force-stopped so the spindle
    // doesn't spin forever unattended.
    bool jogRequested_ = false;
    unsigned long lastJogPingMs_ = 0;
};
