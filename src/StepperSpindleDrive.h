#pragma once
#include <Arduino.h>
#include "config.h"
#include "SpindleDrive.h"

// Default spindle backend: TMC2130 in STANDALONE mode, STEP pin only - no
// DIR (spindle only ever turns one way, DIR is hardwired on the driver
// board) and no SPI. Since there's no direction to manage, this drives the
// STEP pin directly with a micros()-timed pulse generator instead of using
// AccelStepper (which requires a DIR pin).
class StepperSpindleDrive : public ISpindleDrive {
public:
    void begin() override;
    void update() override;

    void setTargetRPM(float rpm) override { targetRpm_ = rpm; }
    float targetRPM() const override { return targetRpm_; }

    void enable() override { enabled_ = true; }
    void disable() override { enabled_ = false; targetRpm_ = 0.0f; currentRpm_ = 0.0f; }

    long takeRotationDeltaPulses() override;
    float pulsesPerRev() const override { return (float)SPINDLE_STEPS_PER_REV; }

private:
    float targetRpm_ = 0.0f;
    float currentRpm_ = 0.0f;
    bool enabled_ = false;

    bool pinHigh_ = false;
    unsigned long lastToggleUs_ = 0;
    unsigned long lastUpdateMs_ = 0;

    long stepCount_ = 0;
    long lastTakenCount_ = 0;
};
