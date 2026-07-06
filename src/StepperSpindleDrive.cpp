#include "StepperSpindleDrive.h"

void StepperSpindleDrive::begin() {
    pinMode(SPINDLE_STEP_PIN, OUTPUT);
    digitalWrite(SPINDLE_STEP_PIN, LOW);
    lastUpdateMs_ = millis();
    lastToggleUs_ = micros();
}

void StepperSpindleDrive::update() {
    unsigned long nowMs = millis();
    float dt = (lastUpdateMs_ == 0) ? 0.0f : (nowMs - lastUpdateMs_) / 1000.0f;
    lastUpdateMs_ = nowMs;

    float target = enabled_ ? targetRpm_ : 0.0f;
    float maxDelta = SPINDLE_ACCEL_RPM_S * dt;
    if (currentRpm_ < target) {
        currentRpm_ = min(target, currentRpm_ + maxDelta);
    } else if (currentRpm_ > target) {
        currentRpm_ = max(target, currentRpm_ - maxDelta);
    }

    if (currentRpm_ <= 0.01f) {
        if (pinHigh_) {
            digitalWrite(SPINDLE_STEP_PIN, LOW);
            pinHigh_ = false;
        }
        return;
    }

    float stepsPerSec = currentRpm_ * SPINDLE_STEPS_PER_REV / 60.0f;
    unsigned long halfPeriodUs = (unsigned long)(500000.0f / stepsPerSec);
    unsigned long now = micros();
    if (now - lastToggleUs_ >= halfPeriodUs) {
        lastToggleUs_ = now;
        pinHigh_ = !pinHigh_;
        digitalWrite(SPINDLE_STEP_PIN, pinHigh_ ? HIGH : LOW);
        if (pinHigh_) stepCount_++;
    }
}

long StepperSpindleDrive::takeRotationDeltaPulses() {
    long delta = stepCount_ - lastTakenCount_;
    lastTakenCount_ = stepCount_;
    return delta;
}
