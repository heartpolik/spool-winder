#include "Display.h"
#include "config.h"
#include <Wire.h>

// 72x40 is too small for readable multi-line text at a glance, so the state
// is shown as a 16x16 icon (glanceable) instead of a word. Only the tiny
// on-device menu (long-press Start/Stop) still uses small text lines.
void Display::begin() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    u8g2_.begin();
}

void Display::drawStateIcon(int x, int y, WinderState state) {
    switch (state) {
        case WinderState::HOMING:
            // target/crosshair = seeking zero position
            u8g2_.drawCircle(x + 8, y + 8, 7);
            u8g2_.drawDisc(x + 8, y + 8, 2);
            break;

        case WinderState::IDLE:
            // hollow circle = standby
            u8g2_.drawCircle(x + 8, y + 8, 7);
            break;

        case WinderState::JOG:
            // up/down arrow pair = manual jog
            u8g2_.drawTriangle(x + 8, y + 1, x + 3, y + 7, x + 13, y + 7);
            u8g2_.drawTriangle(x + 8, y + 15, x + 3, y + 9, x + 13, y + 9);
            break;

        case WinderState::WINDING:
            // right-pointing triangle = play
            u8g2_.drawTriangle(x + 3, y + 2, x + 3, y + 14, x + 13, y + 8);
            break;

        case WinderState::PAUSED:
            // two vertical bars = pause
            u8g2_.drawBox(x + 3, y + 2, 4, 12);
            u8g2_.drawBox(x + 9, y + 2, 4, 12);
            break;

        case WinderState::DONE:
            // checkmark
            u8g2_.drawLine(x + 2, y + 8, x + 6, y + 12);
            u8g2_.drawLine(x + 2, y + 9, x + 6, y + 13);
            u8g2_.drawLine(x + 6, y + 12, x + 14, y + 3);
            u8g2_.drawLine(x + 6, y + 13, x + 14, y + 4);
            break;

        case WinderState::ERROR_RUNOUT:
            // warning triangle with "!"
            u8g2_.drawLine(x + 8, y + 1, x + 1, y + 14);
            u8g2_.drawLine(x + 8, y + 1, x + 15, y + 14);
            u8g2_.drawLine(x + 1, y + 14, x + 15, y + 14);
            u8g2_.drawBox(x + 7, y + 5, 2, 5);
            u8g2_.drawBox(x + 7, y + 12, 2, 2);
            break;
    }
}

void Display::drawRunScreen(const Winder& winder) {
    drawStateIcon(0, 0, winder.state());

    u8g2_.setFont(u8g2_font_fub14_tf);
    u8g2_.setCursor(18, 15);
    u8g2_.printf("%.0f", winder.currentLengthMm());

    // progress bar
    u8g2_.drawFrame(0, 18, 72, 6);
    int fillW = (int)(70.0f * winder.progressPct() / 100.0f);
    if (fillW > 0) u8g2_.drawBox(1, 19, fillW, 4);

    // target length setting (long-press Start/Stop to edit)
    u8g2_.setFont(u8g2_font_5x7_tf);
    u8g2_.setCursor(0, 36);
    u8g2_.printf("L:%.0fmm", winder.targetLengthMm());
}

void Display::drawMenu(const Winder& winder, const String& ipAddress) {
    u8g2_.setFont(u8g2_font_5x7_tf);

    char lengthBuf[16];
    float lengthVal = (winder.isMenuEditing() && winder.menuRow() == 1 &&
                        winder.presets().targetLengthsMm.count > 0)
        ? winder.presets().targetLengthsMm.values[winder.editPresetIndex()]
        : winder.targetLengthMm();
    snprintf(lengthBuf, sizeof(lengthBuf), "L:%.0fmm", lengthVal);

    const char* rows[2] = {ipAddress.c_str(), lengthBuf};

    for (int i = 0; i < 2; i++) {
        int y = 14 + i * 16;
        u8g2_.setCursor(0, y);
        bool selected = (i == winder.menuRow());
        u8g2_.print(selected ? (winder.isMenuEditing() ? "*" : ">") : " ");
        u8g2_.print(rows[i]);
    }
}

void Display::drawConnectionIcon(bool connected) {
    // Small dot in the top-right corner - a station (browser, Cardputer
    // remote, etc) is joined to the AP. Absent when nobody's connected,
    // deliberately unobtrusive on a screen this small.
    if (connected) {
        u8g2_.drawDisc(69, 3, 2);
    }
}

void Display::update(const Winder& winder, const String& ipAddress, bool remoteConnected) {
    unsigned long now = millis();
    if (now - lastDrawMs_ < 150) return; // ~6fps is plenty
    lastDrawMs_ = now;

    u8g2_.clearBuffer();

    if (winder.isMenuActive()) {
        drawMenu(winder, ipAddress);
    } else {
        drawRunScreen(winder);
    }
    drawConnectionIcon(remoteConnected);

    u8g2_.sendBuffer();
}
