#include "FilamentSensor.h"

volatile unsigned long FilamentSensor::pulseCount_ = 0;
volatile unsigned long FilamentSensor::lastPulseMillis_ = 0;

void IRAM_ATTR FilamentSensor::onMotionIsr() {
    pulseCount_++;
    lastPulseMillis_ = millis();
}

void FilamentSensor::begin() {
    pinMode(SFS_MOTION_PIN, INPUT_PULLUP);
    pinMode(SFS_SWITCH_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SFS_MOTION_PIN), onMotionIsr, CHANGE);
    lastPulseMillis_ = millis();
}

void FilamentSensor::setMonitoring(bool active) {
    monitoring_ = active;
    if (active) {
        lastPulseMillis_ = millis();
        stalled_ = false;
    }
}

void FilamentSensor::update() {
    runout_ = (digitalRead(SFS_SWITCH_PIN) != SFS_SWITCH_PRESENT_LEVEL);

    if (monitoring_) {
        unsigned long since = millis() - lastPulseMillis_;
        stalled_ = since > SFS_RUNOUT_TIMEOUT_MS;
    } else {
        stalled_ = false;
    }
}
