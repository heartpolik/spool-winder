#pragma once
#include <Arduino.h>
#include "config.h"

// Wraps BTT SFS V2.0: motion_sensor pin toggles per encoder tick while
// filament moves (length measurement + stall/jam detection), switch_sensor
// pin is a mechanical presence switch (filament physically in/out).
class FilamentSensor {
public:
    void begin();

    // Call every loop() iteration.
    void update();

    float lengthMm() const { return pulseCount_ * SFS_MM_PER_PULSE; }
    void resetLength() { pulseCount_ = 0; }

    // Mechanical switch: filament physically out of the sensor.
    bool isRunout() const { return runout_; }

    // True if winding is active but no motion pulse has arrived within
    // SFS_RUNOUT_TIMEOUT_MS -> jam (filament present but not feeding).
    bool isStalled() const { return stalled_; }

    // Arm/disarm stall detection (call when winding starts/stops).
    void setMonitoring(bool active);

    static void IRAM_ATTR onMotionIsr();

private:
    static volatile unsigned long pulseCount_;
    static volatile unsigned long lastPulseMillis_;

    bool runout_ = false;
    bool stalled_ = false;
    bool monitoring_ = false;
};
