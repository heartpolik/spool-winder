#include "Presets.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

void Presets::loadDefaults() {
    const float l[] = {250, 500, 1000, 2000};
    targetLengthsMm.count = sizeof(l) / sizeof(l[0]);
    for (int i = 0; i < targetLengthsMm.count; i++) targetLengthsMm.values[i] = l[i];
}

void Presets::begin() {
    loadDefaults();

    File f = LittleFS.open(PRESETS_FILE, "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;

    JsonArray l = doc["targetLengthsMm"].as<JsonArray>();
    if (!l.isNull() && l.size() > 0) {
        int n = min((int)l.size(), PresetList::MAX);
        int i = 0;
        for (JsonVariant v : l) {
            if (i >= n) break;
            targetLengthsMm.values[i++] = v.as<float>();
        }
        targetLengthsMm.count = i;
    }
}

bool Presets::save(const PresetList& lengths) {
    JsonDocument doc;
    JsonArray l = doc["targetLengthsMm"].to<JsonArray>();
    for (int i = 0; i < lengths.count; i++) l.add(lengths.values[i]);

    File f = LittleFS.open(PRESETS_FILE, "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();

    targetLengthsMm = lengths;
    return true;
}
