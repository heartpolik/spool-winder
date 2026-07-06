#pragma once
#include <Arduino.h>

// ============================================================================
// Board: 01Space ESP32-C3-0.42LCD (github.com/01Space/ESP32-C3-0.42LCD)
// Onboard OLED (SSD1306, 72x40) is hardwired to GPIO5(SDA)/GPIO6(SCL) - do not
// reassign. GPIO8 = onboard blue LED, GPIO9 = BOOT button (strapping pin,
// avoid driving it low at boot). Only 9 GPIOs are free for everything else:
// 0, 1, 2, 3, 4, 7, 10, 20, 21 - all 9 are spoken for below, no spares left.
//
// That budget does not fit 2x SPI-configured TMC2130 drivers (SPI bus + CS +
// EN per driver = 11 pins alone). So TMC2130s here run in STANDALONE mode:
// no SPI, no TMCStepper lib. Set microstepping via the driver board's
// MS1/MS2 (or CFG) jumpers/resistors and motor current via its trimpot -
// both are hardware settings, not software. STEP/DIR are the only pins the
// MCU drives. Tie each driver's ENN pin to GND on the driver board itself
// (always enabled) since there's no spare GPIO for it; "disable" in this
// firmware just means "commanded speed = 0", not physically de-energized.
// ============================================================================

// ---------- Wi-Fi ----------
#define WIFI_AP_SSID      "FilamentWinder"
#define WIFI_AP_PASS      "winder123"

// ---------- OLED (onboard, SSD1306 72x40, hardwired - do not change) ----------
#define OLED_SDA_PIN      5
#define OLED_SCL_PIN      6

// ---------- Spindle drive backend ----------
// STEPPER (default): TMC2130 standalone (STEP/DIR only, no SPI).
// DC_MOTOR (optional): DC/synchronous motor via PWM, closed-loop RPM from a
// shaft contact sensor. Only one backend is compiled in.
#define SPINDLE_DRIVE_STEPPER   1
#define SPINDLE_DRIVE_DC_MOTOR  2
#define SPINDLE_DRIVE_MODE      SPINDLE_DRIVE_STEPPER

// ---------- Spindle (bobbin) motor - STEPPER mode ----------
// Spindle only ever turns one way (traverse reversal makes the layer-to-layer
// pattern, not the spindle), so there's no DIR pin at all - tie the TMC2130's
// DIR pin directly to GND or VCC on the driver board itself. That frees a
// whole GPIO versus the old always-both-pins stepper wiring.
#define SPINDLE_STEP_PIN  0

// ---------- Spindle (bobbin) motor - DC_MOTOR mode ----------
// Reuses the stepper's STEP pin slot (mutually exclusive builds). No EN pin
// either - "disable" just means PWM duty = 0, same philosophy as everywhere
// else pin budget forced us to drop hardware disable.
#define DC_MOTOR_PWM_PIN         SPINDLE_STEP_PIN   // GPIO0
#define DC_MOTOR_PWM_CHANNEL     0
#define DC_MOTOR_PWM_FREQ_HZ     20000
#define DC_MOTOR_PWM_RES_BITS    10

// Shaft contact sensor (reed/microswitch/hall) for RPM + rotation feedback.
// Only wired/used in DC_MOTOR mode (spindle STEP pin is free then).
#define SPINDLE_ENCODER_PIN                     20
#define SPINDLE_ENCODER_PULSES_PER_REV_DEFAULT  1

// ---------- Traverse (carriage) motor - TMC2130 standalone (STEP/DIR) ----------
#define TRAVERSE_STEP_PIN 2
#define TRAVERSE_DIR_PIN  3

// ---------- Traverse limit switches (left + right end of travel) ----------
// No spool-width setting at all: the carriage just runs until it hits
// whichever end it's heading toward, reverses, and repeats - the coil width
// is whatever's physically between the two endstops. Two physical NO
// switches (one at each end), wired in PARALLEL to one shared GPIO (either
// closing pulls the line to GND). We never need to tell them apart: since
// firmware always knows which direction it's currently driving the
// carriage, a trigger can only be the switch at the end being approached.
// Homing (once at boot) seeks this same pin at slow speed and zeroes there.
#define TRAVERSE_LIMIT_PIN              1
#define TRAVERSE_LIMIT_TRIGGERED_LEVEL  LOW
#define TRAVERSE_HOMING_SPEED_MM_S      5.0f
// Ignore repeat triggers for this long after a flip/homing, so the switch
// being held closed while the carriage clears it doesn't cause a re-flip.
#define TRAVERSE_LIMIT_DEBOUNCE_MS      200

// ---------- Filament sensor (BTT SFS V2.0) ----------
// motion_sensor: toggles while filament moves -> used as encoder pulse.
// switch_sensor: mechanical presence switch -> filament physically in/out.
#define SFS_MOTION_PIN    4
#define SFS_SWITCH_PIN    21

// switch_sensor level when filament IS present (adjust if your unit reads
// the opposite way - depends on the microswitch's NO/NC wiring).
#define SFS_SWITCH_PRESENT_LEVEL  LOW

// ---------- Buttons (active LOW, use internal pullups) ----------
#define BTN_START_STOP_PIN 7
#define BTN_JOG_PIN         10

// ---------- Motion / mechanics ----------
#define MICROSTEPS           16   // must match the driver board's MS1/MS2 jumper setting

// Spindle: full steps/rev of motor * microsteps (no external gearing assumed).
#define SPINDLE_STEPS_PER_REV   (200 * MICROSTEPS)

// Traverse leadscrew: mm of carriage travel per full motor revolution.
#define TRAVERSE_LEAD_MM_PER_REV   8.0f
#define TRAVERSE_STEPS_PER_REV     (200 * MICROSTEPS)
#define TRAVERSE_STEPS_PER_MM      (TRAVERSE_STEPS_PER_REV / TRAVERSE_LEAD_MM_PER_REV)

// SFS encoder calibration: mm of filament per motion-pin toggle.
// Measure empirically: feed a known length, count pulses, divide.
#define SFS_MM_PER_PULSE      2.2f

// If no SFS pulse arrives within this time while winding is active
// (and spindle is turning), treat it as filament runout / jam.
#define SFS_RUNOUT_TIMEOUT_MS  4000

// Winding speed (spindle RPM) and slow-jog RPM for first layer.
#define SPINDLE_WIND_RPM        60.0f
#define SPINDLE_JOG_RPM         8.0f
#define SPINDLE_ACCEL_RPM_S      120.0f

// Debounce for buttons (ms). Long-press threshold for Start/Stop, used to
// enter/exit the on-device menu (short press = start/stop/reset as usual).
#define BUTTON_DEBOUNCE_MS   30
#define BUTTON_LONG_PRESS_MS 600

// Remote clients (web UI, Cardputer app, etc) must keep POSTing
// /api/jog/start at least this often while holding jog, or it auto-stops -
// guards against a dropped connection leaving the spindle jogging forever.
#define JOG_REMOTE_TIMEOUT_MS  800

// The OLED's connection-dot shows as long as *some* API request has landed
// within this window. Comfortably above the web UI's own 1000ms status
// poll interval, so it doesn't flicker off between polls.
#define CLIENT_ACTIVITY_TIMEOUT_MS  2500

// Target-length presets shown in the on-device menu, stored as JSON on
// LittleFS. Editable from the web UI (/api/presets) or by hand-editing this
// file before/after uploadfs. Falls back to built-in defaults if missing or
// malformed.
#define PRESETS_FILE  "/presets.json"
