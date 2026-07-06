#pragma once
#include <Arduino.h>
#include <AccelStepper.h>
#include "config.h"
#include "SpindleDrive.h"
#include "StepperSpindleDrive.h"
#include "DcSpindleDrive.h"

// Drives spindle (bobbin) + traverse (carriage) steppers. Both TMC2130
// drivers run STANDALONE (STEP/DIR only, no SPI - see config.h for why).
// Spindle backend is swappable (SPINDLE_DRIVE_MODE): either the default
// TMC2130 stepper, or an optional DC/synchronous motor with a shaft
// contact-sensor for closed-loop RPM + rotation counting. Traverse position
// is derived from spindle rotation pulses either way, so the carriage moves
// back-and-forth in sync with winding -> layer-to-layer coil. There's no
// spool-width setting - the traverse just runs to whichever end it's
// heading toward (TRAVERSE_LIMIT_PIN) and reverses there.
class MotionControl {
public:
    void begin();
    void update();               // call every loop() iteration

    // Commanded spindle speed in RPM. 0 = stop (motors stay energized -
    // there's no spare GPIO on this board to physically cut driver power).
    void setSpindleRPM(float rpm);
    float spindleRPM() const { return spindleDrive_->targetRPM(); }

    void setFilamentDiameterMm(float mm);

    // Only meaningful when SPINDLE_DRIVE_MODE == SPINDLE_DRIVE_DC_MOTOR;
    // no-op otherwise. Number of contact-sensor pulses per shaft revolution.
    void setSpindleEncoderPulsesPerRev(uint16_t pulses);

    void disableMotors();
    void enableMotors();

    bool isMoving() const { return spindleDrive_->targetRPM() != 0.0f; }

    // Seeks the traverse toward TRAVERSE_LIMIT_PIN at slow speed and zeroes
    // its position there.
    void startHoming();
    bool isHomed() const { return homed_; }
    bool isHoming() const { return homing_; }

private:
    bool limitTriggered() const;

#if SPINDLE_DRIVE_MODE == SPINDLE_DRIVE_DC_MOTOR
    DcSpindleDrive spindleDriveImpl_;
#else
    StepperSpindleDrive spindleDriveImpl_;
#endif
    ISpindleDrive* spindleDrive_ = &spindleDriveImpl_;

    AccelStepper traverse_{AccelStepper::DRIVER, TRAVERSE_STEP_PIN, TRAVERSE_DIR_PIN};
    bool traverseEnabled_ = false;

    float filamentDiameterMm_ = 1.75f;

    bool traverseDirForward_ = true;
    unsigned long lastLimitFlipMs_ = 0;

    bool homed_ = false;
    bool homing_ = false;
};
