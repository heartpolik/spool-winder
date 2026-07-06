#pragma once
#include <Arduino.h>
#include "config.h"

// Debounced digital button, active LOW with internal pullup. Distinguishes
// short press (fires on release, if released before the long-press
// threshold) from long press (fires once, as soon as the threshold is
// crossed while still held - so a long-press action feels immediate rather
// than waiting for release).
class Button {
public:
    void begin(uint8_t pin) {
        pin_ = pin;
        pinMode(pin_, INPUT_PULLUP);
        stableState_ = digitalRead(pin_);
        lastRaw_ = stableState_;
    }

    // Call every loop() iteration.
    void update() {
        int raw = digitalRead(pin_);
        if (raw != lastRaw_) {
            lastChangeMs_ = millis();
            lastRaw_ = raw;
        }
        if ((millis() - lastChangeMs_) > BUTTON_DEBOUNCE_MS && stableState_ != lastRaw_) {
            stableState_ = lastRaw_;
            if (pressed()) {
                pressStartMs_ = millis();
                longFired_ = false;
                justPressed_ = true;
            } else {
                justReleased_ = true;
                if (!longFired_) justShortPress_ = true;
            }
        }

        if (pressed() && !longFired_ && (millis() - pressStartMs_) >= BUTTON_LONG_PRESS_MS) {
            longFired_ = true;
            justLongPress_ = true;
        }
    }

    bool pressed() const { return stableState_ == LOW; }

    // Press-down / release edges - immediate, no long-press ambiguity.
    bool consumeJustPressed() {
        bool v = justPressed_;
        justPressed_ = false;
        return v;
    }

    bool consumeJustReleased() {
        bool v = justReleased_;
        justReleased_ = false;
        return v;
    }

    // Fires on release, only if the press was shorter than BUTTON_LONG_PRESS_MS.
    bool consumeShortPress() {
        bool v = justShortPress_;
        justShortPress_ = false;
        return v;
    }

    // Fires once, while still held, as soon as BUTTON_LONG_PRESS_MS elapses.
    bool consumeLongPress() {
        bool v = justLongPress_;
        justLongPress_ = false;
        return v;
    }

private:
    uint8_t pin_ = 0;
    int stableState_ = HIGH;
    int lastRaw_ = HIGH;
    unsigned long lastChangeMs_ = 0;
    unsigned long pressStartMs_ = 0;
    bool longFired_ = false;
    bool justPressed_ = false;
    bool justReleased_ = false;
    bool justShortPress_ = false;
    bool justLongPress_ = false;
};
