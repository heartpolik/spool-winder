#include "Winder.h"

void Winder::begin() {
    motion_.begin();
    sensor_.begin();
    presets_.begin();
    btnStartStop_.begin(BTN_START_STOP_PIN);
    btnJog_.begin(BTN_JOG_PIN);

    motion_.setFilamentDiameterMm(filamentDiameterMm_);
    motion_.setSpindleEncoderPulsesPerRev(spindleEncoderPulsesPerRev_);
    motion_.enableMotors();
    motion_.startHoming();
}

void Winder::setFilamentDiameterMm(float mm) {
    filamentDiameterMm_ = mm;
    motion_.setFilamentDiameterMm(mm);
}

void Winder::setTargetLengthMm(float mm) {
    targetLengthMm_ = mm;
}

void Winder::setSpindleEncoderPulsesPerRev(uint16_t pulses) {
    spindleEncoderPulsesPerRev_ = pulses > 0 ? pulses : 1;
    motion_.setSpindleEncoderPulsesPerRev(spindleEncoderPulsesPerRev_);
}

void Winder::enterState(WinderState s) {
    state_ = s;
}

void Winder::start() {
    if (state_ == WinderState::IDLE || state_ == WinderState::PAUSED) {
        enterState(WinderState::WINDING);
        sensor_.setMonitoring(true);
    }
}

void Winder::stop() {
    if (state_ == WinderState::WINDING) {
        enterState(WinderState::PAUSED);
        sensor_.setMonitoring(false);
    }
}

void Winder::resetJob() {
    sensor_.resetLength();
    sensor_.setMonitoring(false);
    enterState(WinderState::IDLE);
}

void Winder::handleButtons() {
    btnStartStop_.update();
    btnJog_.update();

    if (state_ == WinderState::HOMING) return;

    // Long-press Start/Stop toggles the on-device menu (only from a resting
    // state - can't open it mid-wind, you have to stop first).
    if (btnStartStop_.consumeLongPress()) {
        if (!menuActive_) {
            if (state_ == WinderState::IDLE || state_ == WinderState::PAUSED ||
                state_ == WinderState::DONE || state_ == WinderState::ERROR_RUNOUT) {
                menuActive_ = true;
                menuRow_ = 0;
                menuEditing_ = false;
            }
        } else {
            menuActive_ = false;
            menuEditing_ = false;
        }
        return;
    }

    if (menuActive_) {
        // Jog navigates: cycles menu rows (0=IP, 1=length), or cycles the
        // preset value while editing.
        if (btnJog_.consumeJustPressed()) {
            if (!menuEditing_) {
                menuRow_ = (menuRow_ + 1) % 2;
            } else if (menuRow_ == 1 && presets_.targetLengthsMm.count > 0) {
                editPresetIndex_ = (editPresetIndex_ + 1) % presets_.targetLengthsMm.count;
            }
        }

        // Play/Start-Stop is "enter" then "confirm": opens the field for
        // editing, then applies the currently-cycled preset on the next press.
        if (btnStartStop_.consumeShortPress()) {
            if (menuRow_ == 1 && presets_.targetLengthsMm.count > 0) {
                if (!menuEditing_) {
                    menuEditing_ = true;
                    editPresetIndex_ = presets_.targetLengthsMm.closestIndex(targetLengthMm_);
                } else {
                    setTargetLengthMm(presets_.targetLengthsMm.values[editPresetIndex_]);
                    menuEditing_ = false;
                }
            }
            // row 0 (IP) is view-only, short-press does nothing there
        }
        return;
    }

    // Jog: hold physical button OR hold remote jog (web/app), release either
    // way to return to prior state. jogActive_ is latched here for update()
    // to read, since the physical button's edge-consuming methods can only
    // be read once.
    bool jogWanted = btnJog_.pressed() || remoteJogActive_;
    if (jogWanted && !jogActive_) {
        if (state_ == WinderState::IDLE || state_ == WinderState::PAUSED) {
            preJogState_ = state_;
            enterState(WinderState::JOG);
        }
    }
    if (!jogWanted && jogActive_) {
        if (state_ == WinderState::JOG) {
            enterState(preJogState_);
        }
    }
    jogActive_ = jogWanted;

    if (btnStartStop_.consumeShortPress()) {
        switch (state_) {
            case WinderState::IDLE:
            case WinderState::PAUSED:
                start();
                break;
            case WinderState::WINDING:
                stop();
                break;
            case WinderState::DONE:
            case WinderState::ERROR_RUNOUT:
                resetJob();
                break;
            default:
                break;
        }
    }
}

void Winder::update() {
    sensor_.update();
    handleButtons();

    switch (state_) {
        case WinderState::HOMING:
            motion_.setSpindleRPM(0);
            if (motion_.isHomed()) {
                enterState(WinderState::IDLE);
            }
            break;
        case WinderState::WINDING: {
            motion_.setSpindleRPM(SPINDLE_WIND_RPM);
            if (sensor_.isRunout() || sensor_.isStalled()) {
                motion_.setSpindleRPM(0);
                sensor_.setMonitoring(false);
                enterState(WinderState::ERROR_RUNOUT);
            } else if (targetLengthMm_ > 0 && currentLengthMm() >= targetLengthMm_) {
                motion_.setSpindleRPM(0);
                sensor_.setMonitoring(false);
                enterState(WinderState::DONE);
            }
            break;
        }
        case WinderState::JOG:
            motion_.setSpindleRPM(jogActive_ ? SPINDLE_JOG_RPM : 0);
            break;
        case WinderState::IDLE:
        case WinderState::PAUSED:
        case WinderState::DONE:
        case WinderState::ERROR_RUNOUT:
        default:
            motion_.setSpindleRPM(0);
            break;
    }

    motion_.update();
}
