#pragma once
#include <Arduino.h>
#include "config.h"

// Contact sensor (reed/microswitch/hall) mounted on the spindle shaft.
// Counts raw pulses; pulsesPerRev is configurable since it depends on how
// many contact points/magnets are on the shaft (1 for a single trip point,
// more for finer resolution).
class SpindleEncoder {
public:
    void begin();
    void update();

    void setPulsesPerRev(uint16_t pulses) { pulsesPerRev_ = pulses > 0 ? pulses : 1; }
    uint16_t pulsesPerRev() const { return pulsesPerRev_; }

    // Pulses accumulated since the last call (consumes them).
    long takeDeltaPulses();

    // Current measured speed, smoothed from pulse intervals.
    float rpm() const { return rpm_; }

    static void IRAM_ATTR onPulseIsr();

private:
    static volatile unsigned long pulseCount_;
    static volatile unsigned long lastPulseUs_;
    static volatile unsigned long prevPulseUs_;

    long lastTakenCount_ = 0;
    uint16_t pulsesPerRev_ = SPINDLE_ENCODER_PULSES_PER_REV_DEFAULT;
    float rpm_ = 0.0f;
};
