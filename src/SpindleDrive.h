#pragma once

// Common interface for the two interchangeable spindle backends:
// StepperSpindleDrive (TMC2130 stepper, open-loop step-commanded) and
// DcSpindleDrive (DC/synchronous motor, PWM + closed-loop RPM feedback).
// MotionControl talks to whichever one is selected in config.h without
// caring which it is - it only needs a target RPM and a rotation-delta
// pulse count to keep the traverse in sync.
class ISpindleDrive {
public:
    virtual ~ISpindleDrive() = default;

    virtual void begin() = 0;
    virtual void update() = 0;

    virtual void setTargetRPM(float rpm) = 0;
    virtual float targetRPM() const = 0;

    virtual void enable() = 0;
    virtual void disable() = 0;

    // Rotation pulses (steps for stepper, encoder ticks for DC motor) since
    // the last call - consumed on read. Used to advance the traverse.
    virtual long takeRotationDeltaPulses() = 0;
    virtual float pulsesPerRev() const = 0;
};
