#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "Winder.h"
#include "Display.h"
#include "WebUI.h"

Winder winder;
Display display;
WebUI webUI;

void setup() {
    Serial.begin(115200);
    LittleFS.begin(true); // mounted once here - Winder::begin() needs it for presets.json
    winder.begin();
    display.begin();
    webUI.begin(winder);
}

void loop() {
    winder.update();
    webUI.update();
    display.update(winder, webUI.ip(), webUI.hasClient());
}
