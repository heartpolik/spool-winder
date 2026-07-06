#pragma once
#include <Arduino.h>

// Fixed-capacity list of target-length presets (mm) shown/cycled in the
// on-device menu.
struct PresetList {
    static constexpr int MAX = 8;
    float values[MAX] = {};
    int count = 0;

    // Index of the preset closest to v - used to seed the menu's edit
    // cursor at whatever's already configured when entering edit mode.
    int closestIndex(float v) const {
        int best = 0;
        float bestDiff = 1e9f;
        for (int i = 0; i < count; i++) {
            float d = fabsf(values[i] - v);
            if (d < bestDiff) {
                bestDiff = d;
                best = i;
            }
        }
        return best;
    }
};

// Loaded from/saved to PRESETS_FILE on LittleFS (JSON:
// {"targetLengthsMm":[...]}). Falls back to built-in defaults if the file is
// missing or malformed.
class Presets {
public:
    void begin();
    bool save(const PresetList& lengths);

    PresetList targetLengthsMm;

private:
    void loadDefaults();
};
