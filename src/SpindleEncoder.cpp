#include "SpindleEncoder.h"

volatile unsigned long SpindleEncoder::pulseCount_ = 0;
volatile unsigned long SpindleEncoder::lastPulseUs_ = 0;
volatile unsigned long SpindleEncoder::prevPulseUs_ = 0;

void IRAM_ATTR SpindleEncoder::onPulseIsr() {
    unsigned long now = micros();
    prevPulseUs_ = lastPulseUs_;
    lastPulseUs_ = now;
    pulseCount_++;
}

void SpindleEncoder::begin() {
    pinMode(SPINDLE_ENCODER_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SPINDLE_ENCODER_PIN), onPulseIsr, FALLING);
}

long SpindleEncoder::takeDeltaPulses() {
    noInterrupts();
    long count = (long)pulseCount_;
    interrupts();
    long delta = count - lastTakenCount_;
    lastTakenCount_ = count;
    return delta;
}

void SpindleEncoder::update() {
    unsigned long last, prev;
    noInterrupts();
    last = lastPulseUs_;
    prev = prevPulseUs_;
    interrupts();

    if (last == 0 || prev == 0) {
        rpm_ = 0.0f;
        return;
    }
    // If no new pulse in a while, decay toward zero instead of holding stale RPM.
    unsigned long sinceLast = micros() - last;
    if (sinceLast > 1000000UL) {
        rpm_ = 0.0f;
        return;
    }
    unsigned long periodUs = last - prev;
    if (periodUs == 0) return;
    float pulseHz = 1000000.0f / (float)periodUs;
    rpm_ = (pulseHz / (float)pulsesPerRev_) * 60.0f;
}
