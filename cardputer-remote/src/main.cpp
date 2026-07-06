// Remote control for the filament winder, running on M5Stack Cardputer Adv.
// Joins the winder's own Wi-Fi AP as a station and talks to its existing
// REST API (GET /api/status|presets, POST /api/start|stop|reset|config|
// jog/start|jog/stop). No firmware changes on the winder side beyond the
// jog endpoints it already exposes for exactly this purpose.
#include <M5Cardputer.h>
#include <M5GFX.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Everything is drawn into this offscreen sprite and pushed to the real
// panel in one blit (pushSprite). Drawing straight to M5Cardputer.Display
// every frame (fillScreen + redraw) flashes black on the real panel each
// time since the SPI transfer isn't instant - the sprite swap is atomic
// from the panel's point of view, so no visible flicker.
static M5Canvas canvas(&M5Cardputer.Display);

struct WinderStatus {
    String state = "-";
    float lengthMm = 0, targetLengthMm = 0, progressPct = 0, filamentDiameterMm = 0;
    bool runout = false;
    bool stalled = false;
    bool valid = false;
};

enum class Mode { RUN, EDIT_LENGTH, EDIT_DIAMETER };

static WinderStatus status;
static Mode mode = Mode::RUN;
static String editBuffer;

static unsigned long lastPollMs = 0;
static unsigned long lastWifiRetryMs = 0;
static unsigned long lastJogHeartbeatMs = 0;
static bool jogHeld = false;

// Edge-detect state for keys we only want to act on once per press.
static bool prevEnter = false, prevR = false, prevL = false, prevD = false, prevDel = false;

static bool httpPost(const String& path) {
    HTTPClient http;
    http.begin(String(WINDER_BASE_URL) + path);
    http.setTimeout(HTTP_TIMEOUT_MS);
    int code = http.POST("");
    http.end();
    return code == 200;
}

static bool httpPostJson(const String& path, const String& jsonBody) {
    HTTPClient http;
    http.begin(String(WINDER_BASE_URL) + path);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);
    int code = http.POST(jsonBody);
    http.end();
    return code == 200;
}

static void pollStatus() {
    HTTPClient http;
    http.begin(String(WINDER_BASE_URL) + "/api/status");
    http.setTimeout(HTTP_TIMEOUT_MS);
    int code = http.GET();
    if (code == 200) {
        JsonDocument doc;
        if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
            status.state = doc["state"].as<String>();
            status.lengthMm = doc["lengthMm"] | 0.0f;
            status.targetLengthMm = doc["targetLengthMm"] | 0.0f;
            status.progressPct = doc["progressPct"] | 0.0f;
            status.filamentDiameterMm = doc["filamentDiameterMm"] | 0.0f;
            status.runout = doc["runout"] | false;
            status.stalled = doc["stalled"] | false;
            status.valid = true;
        }
    } else {
        status.valid = false;
    }
    http.end();
}

static void handleStartStop() {
    if (status.state == "idle" || status.state == "paused") {
        httpPost("/api/start");
    } else if (status.state == "winding") {
        httpPost("/api/stop");
    } else if (status.state == "done" || status.state == "runout") {
        httpPost("/api/reset");
    }
}

static void enterEditMode(Mode m) {
    mode = m;
    if (m == Mode::EDIT_LENGTH) {
        editBuffer = String((int)status.targetLengthMm);
    } else if (m == Mode::EDIT_DIAMETER) {
        editBuffer = String(status.filamentDiameterMm, 2);
    }
}

static void confirmEdit() {
    float val = editBuffer.toFloat();
    if (val > 0) {
        if (mode == Mode::EDIT_LENGTH) {
            httpPostJson("/api/config", "{\"targetLengthMm\":" + String(val, 1) + "}");
        } else if (mode == Mode::EDIT_DIAMETER) {
            httpPostJson("/api/config", "{\"filamentDiameterMm\":" + String(val, 3) + "}");
        }
        pollStatus(); // refresh immediately so the run screen shows the new value
    }
    mode = Mode::RUN;
}

// Handles all keyboard input for one isChange() event. Digit/'.' entry uses
// the keyboard's "word" vector (chars that just became pressed), matching
// M5Stack's own text-input example - it naturally avoids key-repeat spam
// since word only reflects the current change, not a held state.
static void handleKeys() {
    auto ks = M5Cardputer.Keyboard.keysState();
    bool enter = ks.enter;
    bool del = ks.del;
    bool lPressed = M5Cardputer.Keyboard.isKeyPressed('l');
    bool dPressed = M5Cardputer.Keyboard.isKeyPressed('d');

    if (mode == Mode::RUN) {
        if (enter && !prevEnter) handleStartStop();

        bool rPressed = M5Cardputer.Keyboard.isKeyPressed('r');
        if (rPressed && !prevR) httpPost("/api/reset");
        prevR = rPressed;

        if (lPressed && !prevL) enterEditMode(Mode::EDIT_LENGTH);
        if (dPressed && !prevD) enterEditMode(Mode::EDIT_DIAMETER);
    } else {
        // Same key that opened this edit mode cancels it (no save).
        if (mode == Mode::EDIT_LENGTH && lPressed && !prevL) {
            mode = Mode::RUN;
        } else if (mode == Mode::EDIT_DIAMETER && dPressed && !prevD) {
            mode = Mode::RUN;
        } else {
            for (char c : ks.word) {
                if (((c >= '0' && c <= '9') || c == '.') && editBuffer.length() < 8) {
                    editBuffer += c;
                }
            }
            if (del && !prevDel && editBuffer.length() > 0) {
                editBuffer.remove(editBuffer.length() - 1);
            }
            if (enter && !prevEnter) confirmEdit();
        }
    }

    prevEnter = enter;
    prevDel = del;
    prevL = lPressed;
    prevD = dPressed;
}

static void drawScreen(bool wifiOk) {
    auto& d = canvas;
    d.fillScreen(TFT_BLACK);

    d.setTextSize(1);
    d.setCursor(4, 4);
    d.setTextColor(wifiOk ? TFT_GREEN : TFT_RED);
    d.printf("WiFi: %s", wifiOk ? "connected" : "connecting...");

    if (mode != Mode::RUN) {
        d.setTextColor(TFT_WHITE);
        d.setTextSize(2);
        d.setCursor(4, 30);
        d.print(mode == Mode::EDIT_LENGTH ? "Target length (mm):" : "Filament dia (mm):");

        d.setTextSize(3);
        d.setCursor(4, 55);
        d.setTextColor(TFT_YELLOW);
        d.printf("%s_", editBuffer.c_str());

        d.setTextSize(1);
        d.setTextColor(TFT_CYAN);
        d.setCursor(4, 116);
        d.print("Enter=Save  L/D=Cancel  Del=Backspace");
    } else if (!status.valid) {
        d.setTextColor(TFT_YELLOW);
        d.setCursor(4, 24);
        d.print("No response from winder");
    } else {
        d.setTextColor(TFT_WHITE);
        d.setTextSize(2);
        d.setCursor(4, 20);
        d.printf("State: %s", status.state.c_str());

        d.setTextSize(1);
        d.setCursor(4, 44);
        d.printf("Len: %.0f / %.0f mm", status.lengthMm, status.targetLengthMm);

        int barX = 4, barY = 58, barW = 232, barH = 14;
        d.drawRect(barX, barY, barW, barH, TFT_WHITE);
        int fillW = (int)((barW - 2) * status.progressPct / 100.0f);
        if (fillW > 0) d.fillRect(barX + 1, barY + 1, fillW, barH - 2, TFT_GREEN);

        d.setCursor(4, 78);
        d.printf("Filament dia: %.2f mm", status.filamentDiameterMm);

        if (status.runout || status.stalled) {
            d.setTextColor(TFT_RED);
            d.setCursor(4, 92);
            d.print(status.runout ? "RUNOUT!" : "JAM!");
        }

        d.setTextColor(TFT_CYAN);
        d.setCursor(4, 116);
        d.print("Enter=Start/Stop Space=Jog R=Rst L/D=Edit");
    }

    d.pushSprite(0, 0);
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true); // true = enable keyboard
    M5Cardputer.Display.setRotation(1);

    canvas.setColorDepth(16);
    canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
    canvas.setTextColor(TFT_WHITE);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WINDER_SSID, WINDER_PASS);
}

void loop() {
    M5Cardputer.update();

    bool wifiOk = WiFi.status() == WL_CONNECTED;
    unsigned long now = millis();

    if (!wifiOk && now - lastWifiRetryMs > WIFI_RETRY_INTERVAL_MS) {
        lastWifiRetryMs = now;
        WiFi.disconnect();
        WiFi.begin(WINDER_SSID, WINDER_PASS);
    }

    if (wifiOk && now - lastPollMs > POLL_INTERVAL_MS) {
        lastPollMs = now;
        pollStatus();
    }

    if (wifiOk && M5Cardputer.Keyboard.isChange()) {
        handleKeys();
    }

    bool spaceHeld = wifiOk && mode == Mode::RUN && M5Cardputer.Keyboard.isKeyPressed(' ');
    if (spaceHeld && !jogHeld) {
        httpPost("/api/jog/start");
        lastJogHeartbeatMs = now;
        jogHeld = true;
    } else if (spaceHeld && jogHeld && now - lastJogHeartbeatMs > JOG_HEARTBEAT_MS) {
        httpPost("/api/jog/start");
        lastJogHeartbeatMs = now;
    } else if (!spaceHeld && jogHeld) {
        httpPost("/api/jog/stop");
        jogHeld = false;
    }

    drawScreen(wifiOk);
    delay(20);
}
