#pragma once

enum class WinderState {
    HOMING,     // seeking traverse zero-position sensor at boot
    IDLE,       // configured, motors off, waiting for start
    JOG,        // manual slow wind while jog button held
    WINDING,    // auto winding at target RPM
    PAUSED,     // winding paused mid-job (start/stop pressed)
    DONE,       // target length reached
    ERROR_RUNOUT // filament ran out / jammed
};

inline const char* winderStateName(WinderState s) {
    switch (s) {
        case WinderState::HOMING: return "homing";
        case WinderState::IDLE: return "idle";
        case WinderState::JOG: return "jog";
        case WinderState::WINDING: return "winding";
        case WinderState::PAUSED: return "paused";
        case WinderState::DONE: return "done";
        case WinderState::ERROR_RUNOUT: return "runout";
    }
    return "unknown";
}
