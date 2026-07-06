#pragma once
#include <Arduino.h>
#include "config.h"
#include "SpindleEncoder.h"
#include "SpindleDrive.h"

// Optional spindle backend: brushed/synchronous DC motor driven via PWM,
// closed-loop speed-held (not step-commanded) using RPM feedback from a
// shaft contact sensor (SpindleEncoder). Direction is fixed (winding always
// turns one way) - the traverse reversal handles layer direction, not this.
// No hardware enable pin either - "disable" just means PWM duty = 0.
class DcSpindleDrive : public ISpindleDrive {
public:
    void begin() override;
    void update() override;

    void setTargetRPM(float rpm) override { targetRpm_ = rpm; }
    float targetRPM() const override { return targetRpm_; }

    void enable() override;
    void disable() override;

    long takeRotationDeltaPulses() override { return encoder_.takeDeltaPulses(); }
    float pulsesPerRev() const override { return (float)encoder_.pulsesPerRev(); }

    void setPulsesPerRev(uint16_t pulses) { encoder_.setPulsesPerRev(pulses); }
    float measuredRPM() const { return encoder_.rpm(); }

private:
    SpindleEncoder encoder_;

    float targetRpm_ = 0.0f;
    bool enabled_ = false;

    // Simple PI controller: measured RPM -> PWM duty (0..1).
    float duty_ = 0.0f;
    float integral_ = 0.0f;
    unsigned long lastUpdateMs_ = 0;

    static constexpr float KP = 0.02f;
    static constexpr float KI = 0.01f;
    static constexpr float MAX_DUTY = 1.0f;

    void writeDuty(float duty);
};
