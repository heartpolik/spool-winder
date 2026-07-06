#pragma once

// The winder runs its own Wi-Fi AP (see winder's include/config.h
// WIFI_AP_SSID/WIFI_AP_PASS) - this device just joins it as a normal STA
// client. No changes needed on the winder side for this to work.
#define WINDER_SSID "FilamentWinder"
#define WINDER_PASS "winder123"
#define WINDER_BASE_URL "http://192.168.4.1" // winder's AP gateway IP (fixed)

#define POLL_INTERVAL_MS      500
#define WIFI_RETRY_INTERVAL_MS 3000
#define HTTP_TIMEOUT_MS       1500

// Must stay under the winder's JOG_REMOTE_TIMEOUT_MS (800ms default) or the
// winder auto-stops jog between heartbeats.
#define JOG_HEARTBEAT_MS      300
