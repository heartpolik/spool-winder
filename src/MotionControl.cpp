#include "MotionControl.h"
#include "config.h"

void MotionControl::begin() {
    traverse_.setMaxSpeed(TRAVERSE_STEPS_PER_MM * 60.0f); // generous headroom
    traverse_.setAcceleration(TRAVERSE_STEPS_PER_MM * 400.0f);
    traverse_.setCurrentPosition(0);

    pinMode(TRAVERSE_LIMIT_PIN, INPUT_PULLUP);

    spindleDrive_->begin();
}

void MotionControl::setFilamentDiameterMm(float mm) {
    filamentDiameterMm_ = mm;
}

void MotionControl::setSpindleEncoderPulsesPerRev(uint16_t pulses) {
#if SPINDLE_DRIVE_MODE == SPINDLE_DRIVE_DC_MOTOR
    spindleDriveImpl_.setPulsesPerRev(pulses);
#else
    (void)pulses;
#endif
}

void MotionControl::enableMotors() {
    traverseEnabled_ = true;
    spindleDrive_->enable();
}

void MotionControl::disableMotors() {
    traverseEnabled_ = false;
    spindleDrive_->disable();
}

void MotionControl::setSpindleRPM(float rpm) {
    spindleDrive_->setTargetRPM(rpm);
}

void MotionControl::startHoming() {
    homing_ = true;
    homed_ = false;
    traverse_.setSpeed(-(TRAVERSE_STEPS_PER_MM * TRAVERSE_HOMING_SPEED_MM_S));
}

bool MotionControl::limitTriggered() const {
    return digitalRead(TRAVERSE_LIMIT_PIN) == TRAVERSE_LIMIT_TRIGGERED_LEVEL;
}

void MotionControl::update() {
    if (homing_) {
        spindleDrive_->update(); // Winder keeps spindle RPM at 0 during homing
        if (limitTriggered()) {
            traverse_.setCurrentPosition(0);
            traverseDirForward_ = true;
            lastLimitFlipMs_ = millis();
            homing_ = false;
            homed_ = true;
        } else if (traverseEnabled_) {
            traverse_.runSpeed();
        }
        return;
    }

    spindleDrive_->update();

    // Sync traverse position to spindle rotation delta (works identically
    // whether the delta comes from stepper steps or DC-motor encoder ticks).
    long deltaPulses = spindleDrive_->takeRotationDeltaPulses();
    if (deltaPulses != 0) {
        float pulsesPerRev = spindleDrive_->pulsesPerRev();
        float traverseStepsPerPulse =
            (filamentDiameterMm_ * TRAVERSE_STEPS_PER_MM) / pulsesPerRev;
        long signedDelta = traverseDirForward_ ? deltaPulses : -deltaPulses;
        long moveSteps = (long)lroundf(signedDelta * traverseStepsPerPulse);
        if (moveSteps != 0) {
            traverse_.move(moveSteps);
        }
    }

    if (traverseEnabled_) {
        traverse_.run();
    }

    // Whichever end the carriage is currently heading toward is the only
    // one it can physically be touching, so a trigger always means "flip".
    if (limitTriggered() && (millis() - lastLimitFlipMs_) > TRAVERSE_LIMIT_DEBOUNCE_MS) {
        traverseDirForward_ = !traverseDirForward_;
        lastLimitFlipMs_ = millis();
    }
}
