#pragma once
#include <Arduino.h>
#include "config.h"
#include "WinderState.h"
#include "MotionControl.h"
#include "FilamentSensor.h"
#include "Buttons.h"
#include "Presets.h"

// Top-level orchestrator: owns motion, sensor and button inputs, runs the
// winding state machine and the on-device menu, and is the single source of
// truth for filament diameter / target length. Display and WebUI just read
// from a Winder reference - they don't own state. There's no spool-width
// setting: the traverse just runs between its two physical limit switches.
class Winder {
public:
    void begin();
    void update();

    // ---- configuration (settable via web UI) ----
    void setFilamentDiameterMm(float mm);
    void setTargetLengthMm(float mm);
    void setSpindleEncoderPulsesPerRev(uint16_t pulses);

    float filamentDiameterMm() const { return filamentDiameterMm_; }
    float targetLengthMm() const { return targetLengthMm_; }
    uint16_t spindleEncoderPulsesPerRev() const { return spindleEncoderPulsesPerRev_; }
    bool spindleDriveIsDcMotor() const { return SPINDLE_DRIVE_MODE == SPINDLE_DRIVE_DC_MOTOR; }

    // ---- commands (buttons or web UI) ----
    void start();     // begin/resume winding
    void stop();      // pause winding (keeps progress)
    void resetJob();  // clear progress, go IDLE

    // Network equivalent of holding the physical Jog button - ORed with the
    // real button so either source can drive JOG state. Caller (WebUI) is
    // responsible for clearing this if the connection drops, otherwise the
    // spindle keeps jogging forever.
    void setRemoteJog(bool active) { remoteJogActive_ = active; }

    WinderState state() const { return state_; }
    float currentLengthMm() const { return sensor_.lengthMm(); }
    float progressPct() const {
        return targetLengthMm_ > 0 ? min(100.0f, 100.0f * currentLengthMm() / targetLengthMm_) : 0.0f;
    }

    // ---- on-device menu (long-press Start/Stop to open/close; Jog
    // navigates, short-press Start/Stop enters/confirms an edit) ----
    bool isMenuActive() const { return menuActive_; }
    bool isMenuEditing() const { return menuEditing_; }
    int menuRow() const { return menuRow_; }               // 0=IP, 1=length
    int editPresetIndex() const { return editPresetIndex_; }

    MotionControl& motion() { return motion_; }
    FilamentSensor& sensor() { return sensor_; }
    const FilamentSensor& sensor() const { return sensor_; }
    Presets& presets() { return presets_; }
    const Presets& presets() const { return presets_; }

private:
    void enterState(WinderState s);
    void handleButtons();

    MotionControl motion_;
    FilamentSensor sensor_;
    Presets presets_;
    Button btnStartStop_;
    Button btnJog_;

    WinderState state_ = WinderState::HOMING;
    WinderState preJogState_ = WinderState::IDLE;

    bool menuActive_ = false;
    bool menuEditing_ = false;
    int menuRow_ = 0;
    int editPresetIndex_ = 0;

    bool remoteJogActive_ = false;
    bool jogActive_ = false; // btnJog_.pressed() || remoteJogActive_, latched each tick

    float filamentDiameterMm_ = 1.75f;
    float targetLengthMm_ = 1000.0f;
    uint16_t spindleEncoderPulsesPerRev_ = SPINDLE_ENCODER_PULSES_PER_REV_DEFAULT;
};
