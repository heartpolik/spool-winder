#include "WebUI.h"
#include "config.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>

void WebUI::begin(Winder& winder) {
    winder_ = &winder;

    // LittleFS is mounted once in main.cpp setup(), before Winder::begin()
    // (which needs it to load presets.json) - not re-mounted here.

    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    ip_ = WiFi.softAPIP().toString();

    setupRoutes();
    server_.begin();
}

void WebUI::update() {
    if (jogRequested_ && (millis() - lastJogPingMs_) > JOG_REMOTE_TIMEOUT_MS) {
        jogRequested_ = false;
        winder_->setRemoteJog(false);
    }
}

bool WebUI::hasClient() const {
    return (millis() - lastApiActivityMs_) < CLIENT_ACTIVITY_TIMEOUT_MS;
}

void WebUI::setupRoutes() {
    server_.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        noteActivity();
        JsonDocument doc;
        doc["state"] = winderStateName(winder_->state());
        doc["lengthMm"] = winder_->currentLengthMm();
        doc["targetLengthMm"] = winder_->targetLengthMm();
        doc["progressPct"] = winder_->progressPct();
        doc["filamentDiameterMm"] = winder_->filamentDiameterMm();
        doc["runout"] = winder_->sensor().isRunout();
        doc["stalled"] = winder_->sensor().isStalled();
        doc["spindleDriveIsDcMotor"] = winder_->spindleDriveIsDcMotor();
        doc["spindleEncoderPulsesPerRev"] = winder_->spindleEncoderPulsesPerRev();
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    server_.on("/api/start", HTTP_POST, [this](AsyncWebServerRequest* req) {
        noteActivity();
        winder_->start();
        req->send(200, "application/json", "{\"ok\":true}");
    });

    server_.on("/api/stop", HTTP_POST, [this](AsyncWebServerRequest* req) {
        noteActivity();
        winder_->stop();
        req->send(200, "application/json", "{\"ok\":true}");
    });

    server_.on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest* req) {
        noteActivity();
        winder_->resetJob();
        req->send(200, "application/json", "{\"ok\":true}");
    });

    server_.on("/api/jog/start", HTTP_POST, [this](AsyncWebServerRequest* req) {
        noteActivity();
        jogRequested_ = true;
        lastJogPingMs_ = millis();
        winder_->setRemoteJog(true);
        req->send(200, "application/json", "{\"ok\":true}");
    });

    server_.on("/api/jog/stop", HTTP_POST, [this](AsyncWebServerRequest* req) {
        noteActivity();
        jogRequested_ = false;
        winder_->setRemoteJog(false);
        req->send(200, "application/json", "{\"ok\":true}");
    });

    auto* configHandler = new AsyncCallbackJsonWebHandler("/api/config",
        [this](AsyncWebServerRequest* req, JsonVariant& json) {
            noteActivity();
            JsonObject obj = json.as<JsonObject>();
            if (obj["filamentDiameterMm"].is<float>()) {
                winder_->setFilamentDiameterMm(obj["filamentDiameterMm"].as<float>());
            }
            if (obj["targetLengthMm"].is<float>()) {
                winder_->setTargetLengthMm(obj["targetLengthMm"].as<float>());
            }
            if (obj["spindleEncoderPulsesPerRev"].is<uint16_t>()) {
                winder_->setSpindleEncoderPulsesPerRev(obj["spindleEncoderPulsesPerRev"].as<uint16_t>());
            }
            req->send(200, "application/json", "{\"ok\":true}");
        });
    server_.addHandler(configHandler);

    server_.on("/api/presets", HTTP_GET, [this](AsyncWebServerRequest* req) {
        noteActivity();
        JsonDocument doc;
        JsonArray l = doc["targetLengthsMm"].to<JsonArray>();
        for (int i = 0; i < winder_->presets().targetLengthsMm.count; i++) {
            l.add(winder_->presets().targetLengthsMm.values[i]);
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    auto* presetsHandler = new AsyncCallbackJsonWebHandler("/api/presets",
        [this](AsyncWebServerRequest* req, JsonVariant& json) {
            noteActivity();
            JsonObject obj = json.as<JsonObject>();

            PresetList lengths;
            JsonArray l = obj["targetLengthsMm"].as<JsonArray>();
            if (!l.isNull()) {
                lengths.count = min((int)l.size(), PresetList::MAX);
                int i = 0;
                for (JsonVariant v : l) {
                    if (i >= lengths.count) break;
                    lengths.values[i++] = v.as<float>();
                }
            }

            bool ok = lengths.count > 0 && winder_->presets().save(lengths);
            req->send(ok ? 200 : 400, "application/json",
                      ok ? "{\"ok\":true}" : "{\"ok\":false}");
        });
    server_.addHandler(presetsHandler);
}
