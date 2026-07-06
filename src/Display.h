#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "Winder.h"

// Onboard OLED of the 01Space ESP32-C3-0.42LCD: SSD1306, 72x40 px, I2C.
class Display {
public:
    void begin();
    void update(const Winder& winder, const String& ipAddress, bool remoteConnected);

private:
    void drawStateIcon(int x, int y, WinderState state);
    void drawRunScreen(const Winder& winder);
    void drawMenu(const Winder& winder, const String& ipAddress);
    void drawConnectionIcon(bool connected);

    U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2_{U8G2_R0, U8X8_PIN_NONE};
    unsigned long lastDrawMs_ = 0;
};
