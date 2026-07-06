# Cardputer Remote

Remote control for the filament winder on an M5Stack Cardputer Adv. Joins the winder's own Wi-Fi AP as a plain STA client and talks to its existing REST API — **no firmware changes needed on the winder** beyond the `/api/jog/start`/`/api/jog/stop` endpoints it already exposes for this.

## Build

```
pio run -e cardputer-adv -t upload
pio device monitor
```

## Controls

**Run screen:**
- **Enter** — context action: Start (idle/paused) → Stop (winding) → Reset (done/runout)
- **Space** (hold) — jog, same as holding the winder's physical Jog button. Sends a heartbeat every `JOG_HEARTBEAT_MS` (300ms) while held; the winder auto-stops jog if that heartbeat goes quiet for `JOG_REMOTE_TIMEOUT_MS` (800ms) — a dropped Wi-Fi connection can't leave the spindle jogging forever.
- **R** — force reset regardless of state
- **L** — edit target length
- **D** — edit filament diameter

**Edit mode** (after L or D): type digits and `.` to build the value, **Del** removes the last character, **Enter** saves (`POST /api/config`) and returns to the run screen, pressing **the same key that opened it** (L or D again) cancels without saving.

## Config

`src/config.h`: `WINDER_SSID`/`WINDER_PASS` must match the winder's `WIFI_AP_SSID`/`WIFI_AP_PASS` (`include/config.h` in the main project, defaults `FilamentWinder`/`winder123`). `WINDER_BASE_URL` is the AP gateway IP, `192.168.4.1` by default — only change it if you reconfigured the winder's AP subnet.

## Not implemented

- Bluetooth transport (Wi-Fi only, since the winder doesn't run a BLE server)
- Spool-width setting (the winder doesn't have one either — see its own README, traverse just runs between two limit switches)
