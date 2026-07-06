# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware (PlatformIO/Arduino, C++) for a filament coil winder built on the **01Space ESP32-C3-0.42LCD** board (onboard 72x40 SSD1306 OLED). Drives a spindle motor (bobbin) and a traverse motor (carriage) to wind filament layer-to-layer, tracks fed length via a filament sensor, and exposes a web UI for configuration.

## Commands

```
pio run -e esp32-c3 -t uploadfs   # upload data/ (web UI) to LittleFS - do this before/after changing data/index.html
pio run -e esp32-c3 -t upload     # build + flash firmware
pio device monitor                 # serial monitor, 115200 baud
```

No test suite. No linter configured. There is no `pio` CLI in the dev sandbox this project was built in — verify builds by cross-checking `#define` usage against `include/config.h` (see below) since clangd here can't resolve `Arduino.h`/library headers without a real PlatformIO build environment.

## Hardware constraint that shapes the whole design

The MCU board (01Space ESP32-C3-0.42LCD) has its OLED hardwired to GPIO5/6, onboard LED on GPIO8, BOOT button on GPIO9 — leaving **only 9 free GPIOs: 0, 1, 2, 3, 4, 7, 10, 20, 21**, all of which are already allocated in `include/config.h` (no spares). This is why:

- TMC2130 drivers run in **STANDALONE mode** (no SPI, no `TMCStepper` lib). Microstepping is set via the driver board's MS1/MS2 jumpers to match `MICROSTEPS`, current via the driver's trimpot — both hardware settings, not software. Driver ENN pins are tied to GND on the driver board itself; there's no GPIO for it, so "disable" in firmware just means commanded speed = 0.
- The **spindle has no DIR pin at all** — it only ever turns one way (traverse reversal makes the layer-to-layer pattern, not the spindle), so its DIR is hardwired on the driver board and `StepperSpindleDrive` drives its STEP pin with its own `micros()`-timed pulse generator instead of `AccelStepper` (which requires a DIR pin). That freed GPIO is spent on `TRAVERSE_LIMIT_PIN` instead. Traverse still uses `AccelStepper` since it genuinely needs bidirectional motion.
- There's **no spool-width setting** either — two physical NO limit switches (one at each end of traverse travel) are wired in parallel to that single `TRAVERSE_LIMIT_PIN`, and firmware never needs to tell them apart: it always knows which direction it's currently driving the carriage, so a trigger can only be the switch at the end being approached. This is why one GPIO covers two physical switches.
- The optional DC-motor spindle backend reuses the stepper's STEP pin slot (`config.h` `DC_MOTOR_PWM_PIN` `#define`d to the same pin) since only one backend is ever compiled in. It has no enable pin either — "disable" just means PWM duty = 0.

When adding any new I/O, check `include/config.h`'s pin budget comment first — there is no room to add a pin without freeing one elsewhere (e.g. via an I2C GPIO expander on the shared OLED bus, not currently implemented).

## Architecture

**`Winder`** (`src/Winder.h/.cpp`) is the top-level orchestrator: owns `MotionControl`, `FilamentSensor`, `Presets`, and the two `Button`s, runs the winding state machine (`WinderState`: HOMING/IDLE/JOG/WINDING/PAUSED/DONE/ERROR_RUNOUT) plus the on-device menu (`menuActive_`/`menuEditing_`/`menuRow_`/`editPresetIndex_` — an overlay on top of the job state, not a `WinderState` itself), and is the single source of truth for filament diameter / target length. Both `Display` and `WebUI` just read from a `Winder` reference — they don't own state.

**Button semantics** (`src/Buttons.h`) distinguish short vs. long press: `consumeShortPress()` fires on release only if released before `BUTTON_LONG_PRESS_MS`; `consumeLongPress()` fires once, immediately, as soon as the threshold is crossed while still held (so menu-open feels instant, not release-gated). `Winder::handleButtons()` routes Start/Stop long-press to open/close the menu, and — depending on `menuActive_` — routes Jog/Start/Stop short-presses to either menu navigation/edit or the normal start/stop/jog/reset actions. Jog itself is level-based, not edge-based: `jogWanted = btnJog_.pressed() || remoteJogActive_` (the latter set by `Winder::setRemoteJog()`, driven by `WebUI`'s `/api/jog/*` handlers) — physical button and remote client both just hold a "jog wanted" line high, and `Winder` reacts to that combined level the same way regardless of source.

**`MotionControl`** (`src/MotionControl.h/.cpp`) drives the traverse (carriage) stepper directly via `AccelStepper` (it genuinely needs bidirectional motion), and delegates spindle control to whichever `ISpindleDrive` implementation is selected by `SPINDLE_DRIVE_MODE` in `config.h`:
- `StepperSpindleDrive` — TMC2130 standalone, STEP-pin-only (no DIR — see pin-budget section), driven by its own `micros()`-timed pulse generator, not `AccelStepper`
- `DcSpindleDrive` — PWM-driven DC/synchronous motor, closed-loop RPM held via a PI controller (`KP`/`KI` constants at top of `DcSpindleDrive.h`), fed by `SpindleEncoder` (a shaft contact sensor: reed/microswitch/hall, pulses/rev configurable, not hardcoded)

Key mechanism: **traverse pitch is derived from spindle rotation; traverse direction is derived from limit switches.** Every loop, `MotionControl::update()` asks the active `ISpindleDrive` for a rotation-delta pulse count (`takeRotationDeltaPulses()` — stepper steps or DC-motor encoder ticks depending on backend) and `pulsesPerRev()`, converts that into traverse-motor steps via `filamentDiameterMm_` and `TRAVERSE_STEPS_PER_MM` (this is what makes each spindle revolution advance the carriage by one filament diameter), and separately checks `TRAVERSE_LIMIT_PIN` each tick to flip `traverseDirForward_` when the carriage reaches an end (debounced by `TRAVERSE_LIMIT_DEBOUNCE_MS` so the switch being held closed while clearing it doesn't cause a re-flip). No spool-width bound is computed — it works identically regardless of which spindle backend is compiled in, that's the point of the `ISpindleDrive` interface (`src/SpindleDrive.h`).

**Homing**: `MotionControl::startHoming()`/`isHomed()`/`isHoming()` run a non-blocking seek-to-`TRAVERSE_LIMIT_PIN` routine at boot so the traverse's position-0 and initial direction are real physical references instead of "wherever it was at power-on". `Winder` starts in `WinderState::HOMING` and transitions to `IDLE` once `motion_.isHomed()` — buttons are ignored during this state.

**`FilamentSensor`** (`src/FilamentSensor.h/.cpp`) wraps a BTT SFS V2.0, which has two independent signal pins (not one — this was a wrong assumption fixed later): `motion_sensor` (interrupt-driven pulse counter → length fed, via `SFS_MM_PER_PULSE`; also drives stall/jam detection if no pulse arrives within `SFS_RUNOUT_TIMEOUT_MS` while `setMonitoring(true)`) and `switch_sensor` (a plain mechanical presence switch → `isRunout()`; polarity set by `SFS_SWITCH_PRESENT_LEVEL`, flip it if a given unit reads inverted).

**`Presets`** (`src/Presets.h/.cpp`) holds the target-length values the on-device menu cycles through (`PresetList`, fixed-capacity array + `closestIndex()` to seed the edit cursor). Loaded from/saved to `PRESETS_FILE` (`/presets.json`) on LittleFS, falling back to built-in defaults if missing/malformed. Editable via `GET/POST /api/presets` in `WebUI`, or by hand-editing the JSON file. LittleFS is mounted once in `main.cpp setup()`, before `Winder::begin()` — `Presets::begin()` depends on that ordering.

**`WebUI`** (`src/WebUI.h/.cpp`) serves `data/index.html` from LittleFS and a small JSON REST API (`GET /api/status|presets`, `POST /api/start|stop|reset|config|presets|jog/start|jog/stop`) via ESPAsyncWebServer. Runs as a Wi-Fi AP (`WIFI_AP_SSID`/`WIFI_AP_PASS` in `config.h`), not STA — no router dependency. `hasClient()` is request-recency based (`< CLIENT_ACTIVITY_TIMEOUT_MS` since any `/api/*` call, tracked via `noteActivity()` called at the top of every handler) — used to drive the OLED's connection dot. Deliberately *not* `WiFi.softAPgetStationNum()`: that count is known to stick on ESP32 when a station disappears without a clean disconnect (walks out of range, app killed), so it doesn't reliably return to 0. The `/api/jog/*` pair mirrors the physical Jog button for remote clients (see `Winder::setRemoteJog()` below); `WebUI::update()` force-stops it if a client's heartbeat goes quiet for `JOG_REMOTE_TIMEOUT_MS`, so a dropped connection can't leave the spindle jogging forever. [`cardputer-remote/`](cardputer-remote/) is a separate PlatformIO project (M5Stack Cardputer Adv, ESP32-S3) that drives the winder purely through this REST API as a Wi-Fi STA client of the winder's own AP — no shared code, no changes to the winder's network topology.

**`Display`** (`src/Display.h/.cpp`) draws to the tiny 72x40 OLED. The 72x40 screen is too small for readable words at a glance, so `drawRunScreen()` uses a hand-drawn 16x16 icon (`drawStateIcon()`, primitives not bitmap fonts — circle/triangle/box/line calls per `WinderState`) for status instead of text, plus large digits for fed length, a progress bar, and small text for target length. `drawMenu()` renders the separate 2-row menu screen (IP / length) when `winder.isMenuActive()`. No IP or percentage on the run screen by design — only in the menu. `drawConnectionIcon()` overlays a small dot top-right on either screen when `WebUI::hasClient()` is true.

## Gotchas for future changes

- `include/config.h` is the single place pins, motion constants (steps/rev, mm/pulse, RPMs, timeouts), the `SPINDLE_DRIVE_MODE` compile-time switch, and homing/menu/preset constants live. Read it before touching any hardware-facing code — **all 9 free GPIOs are spoken for, adding one means freeing one elsewhere.**
- `data/index.html` is a separate static asset uploaded via `uploadfs`, not compiled into firmware — changes there need a separate flash step and won't show up from a plain `-t upload`.
- The `ISpindleDrive` interface is the extension point if a third spindle backend is ever added — implement `begin/update/setTargetRPM/targetRPM/enable/disable/takeRotationDeltaPulses/pulsesPerRev` and wire it into the `#if SPINDLE_DRIVE_MODE == ...` block in `MotionControl.h`.
- Menu state (`menuActive_` etc.) is intentionally orthogonal to `WinderState` — opening the menu doesn't change `state_`, so a paused job stays paused while you browse. Don't fold menu mode into the `WinderState` enum; `Winder::handleButtons()` already branches on `menuActive_` before reaching the normal per-state button logic.
