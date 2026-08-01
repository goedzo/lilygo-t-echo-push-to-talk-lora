# Agent Instructions — Main Firmware (LilyGO T-Echo)

## Purpose

Takes care of the T-Echo firmware (nRF52840, SX1262, BLE, 9 modes: BEACON, RAW, TXT, RANGE, TST, PONG, SCAN, PTT, WP) — 37 source modules + crash_debug.h. Audio is entirely handled by the phone app; the T-Echo relays Opus frames via BLE ↔ LoRa only.

## Ownership

- **Entry file:** `main/main.ino`
- **Mode logic:** `main/app_modes.cpp/.h`
- **Radio:** `main/lora.cpp/.h`
- **Display:** `main/display.cpp/.h` (GxDEPG0150BN 1.54" e-paper, GxEPD2)
- **Display Layout:** `main/display_layout.cpp/.h` — per-mode drawXxxLayout() functions
- **Text Inbox:** `main/text_inbox.cpp/.h` — Scrollable message inbox stored in RTC RAM (8 messages × 256 bytes, address 0x200075C0) with sender/timestamp. 16-line scroll display on E-Paper. Auto-stored on incoming TXT/TXT_MULTI packets
- **TXT inbox toggle:** `main/app_modes.cpp` — global `txtShowInbox` (default false). MODE click in TXT mode toggles between single-message and inbox views (`drawTxtInboxLayout()` vs `drawTxtSingleLayout()`). When inbox is closed and messages exist, latest message preview included in screen sync payload.
- **BLE GATT:** `main/ble.cpp/.h` — relays Opus frames from phone → LoRa, and received LoRa audio → phone via BLE notify (binary)
- **Packet framing:** `main/packet.cpp/.h`
- **Settings (RTC):** `main/settings.cpp/.h` (PCF8563)
- **GPS:** `main/gps.cpp/.h` (TinyGPSPlus)
- **Battery:** `main/battery.cpp/.h`
- **Scan/OTA:** `main/scan.cpp/.h` — Frequency scanner (top 10 channels by quality)
- **Screen sync:** `main/screen_sync.cpp/.h` — syncs display state between device and companion app. Three functions: `sendScreenSync()` (1s throttle, battery-change filter), `sendScreenSyncIfDirty()` (dirty-flag driven, BLE-connected guard), `sendScreenSyncForced()` (bypasses all throttling, called on GETSCREEN/GCCREN GATT actions). Payload format: `LINE:S|M:{mode}|H:{channel_sf}|C:{content_fields}|S:{freq}|T:{time}|G:{sats}|B:{batt}%|I:{icons}`. Per-mode content fields documented below.
- **Buddy list:** `main/buddy_list.cpp/.h` — name→device_id buddy list (16 contacts, address 0x20007C00)
- **Display refresh helpers:** `main/disp_refresh.cpp/.h`, `main/disp_timer.cpp/.h` — partial/full refresh control and non-blocking display updates
- **Boot animation:** `main/boot_animation.cpp/.h` — device boot sequence rendering
- **Pin definitions:** `main/utilities.h`
- **Crash detection & debugging:** `main/crash_debug.h` — HardFault recorder, stack overflow guard, debug log buffer, heap tracker

## Local Contracts

- Board: nRF52840 (LilyGO T-Echo PCA10056) targeting `adafruit:nrf52:pca10056`
- **Default revision is active** — VERSION_1 pins are commented out in `utilities.h`
- Active pins: ePaper_Miso=P1.6, LoRa_Dio0=P0.22, GreenLed_Pin=P1.1, RedLed_Pin=P1.3, BlueLed_Pin=P0.14
- Compile via **Arduino CLI only** — PlatformIO is not functional for this firmware

### Serial monitoring (NON-NEGOTIABLE)

**Never use `Serial.println()` or `Serial.print()` in any module source file.** These write only to USB and are invisible in the companion app.

Every debug/log output must use:
- `sendSerialToAppLn(const String&)` — **both** USB serial AND BLE companion app (auto-appends `\n`)
- `sendSerialToApp(const String&)` — **BLE only** (no USB, no newline)

Exceptions (these must go to USB for DFU/crash capture):
- Crash/debug code paths (hardfault, stack guard, heap tracker)
- The internal body of `sendSerialToAppLn` itself in ble.cpp

Before marking any firmware change complete, verify **zero** new/modified `Serial.println()` or `Serial.print()` calls in the changed `.cpp`/`.ino` files.

### Display updates

Call `updDisp()` from app_modes or other modules that change state.
- Header guards: `#pragma once`

### Build instructions (Arduino CLI)

```bash
arduino-cli compile -b adafruit:nrf52:pca10056 --build-path .pio/t-echo-build main
arduino-cli upload -b adafruit:nrf52:pca10056 --port auto .pio/t-echo-build/main.bin
```

**No platformio.ini exists on disk** — any references are stale. Use Arduino CLI only.

## Build Gotchas

- E-paper is **GxDEPG0150BN**. The vendored header is `epd/GxEPD2_150_BN.h`. If it changes, update both `main.ino` and `display.cpp`.
- **DFU upload**: double-click reset button to enter DFU mode first.
- nRF5-SDK overwrites the Adafruit bootloader — do not mix toolchains without restoring bootloader.

### Screen layout (fixed Y positions)

- Top bar: y=`disp_top_margin` (12px), 16px tall, black background with white text. Text cursor: `y = disp_top_margin + 11` (baseline offset for FreeMonoBold9pt7b — centers text in the 16px bar).
- Body area: y=32 to y=168, white background with black text.
- Bottom status bar: y=`disp_height - 32` (168px), 32px tall, black background with white text. Text cursor Y offset: `+20` below the bar top.

### Rendering model — partial updates with mode-switch full refresh

Rendering uses `firstPage()/nextPage()` page-loop pattern inside a shared `renderPageLoop()` helper. 

- **Mode switch**: `forceFullRefresh()` sets a flag → next `drawXxxLayout()` call uses `setFullWindow()` + `refresh(false)` (~2.5s). This clears ghosting artifacts from the previous layout.
- **Normal updates**: All subsequent draws use `setPartialWindow(0, 12, 200, 184)` + `refresh(true)` (~0.8s) — only updates the header/body/statusbar region without full panel refresh.
- The flag is consumed atomically by `pendingFullRefresh()` — returns true once then resets to false.
- Every layout function (`drawDefaultLayout`, `drawBeaconLayout`, etc.) calls `pendingFullRefresh()` at entry and passes the result to `renderPageLoop()`.

`clearScreen()` is a no-op. `updModeAndChannelDisplay()` just calls `drawDefaultLayout()` — it does not do its own refresh.

## Work Guidance

### Mode logic (`app_modes`)

Nine modes: RAW (raw packet dump), TXT (text messages — input via BLE app; inbox with scrollable message history on-device, MODE click toggles `txtShowInbox` view), RANGE (distance testing with GPS sender/receiver roles), TST (auto test beeps every 5s), PONG (manual ping-pong), SCAN (frequency scanner, top 10 channels), PTT (push-to-talk — Opus frames relayed from phone via BLE GATT to LoRa), BEACON (peer roster — broadcasts GPS/battery beacon, displays list of detected peers with distances and battery levels), WP (waypoint — touch to save current GPS as waypoint packet, broadcast on LoRa for 60s). Button/AceButton handles double-click for SF adjustment and long-press for power-off.

### Named Contacts / Call Sign System

Every beacon packet includes an optional `~CN{call_sign}` field (up to 16 ASCII chars). The device stores a buddy list of name→device_id mappings in RTC RAM (16 contacts, address 0x20007C00). On receiving a beacon with `~CN`, the call sign is stored in the roster entry and in the local buddy list. The e-paper roster screen shows call signs instead of hex IDs when available. The companion app can set its own display name via `SETNAME` GATT action, and sync the full buddy list via `GETBUDDY`/`SETBUDDY` actions. New source: `buddy_list.h` / `buddy_list.cpp`.

### Packet framing by mode

| Mode | Header | Payload |
|---|---|---|
| BEACON | `B{id_short}~CN{call_sign}~GP{lat},{lon}~BT{pct}` or `B{id_short}~CN{call_sign}~BT{pct}` (no GPS) | Device ID + call sign + lat/lon/battery (peer roster via distanceBetween) |
| TXT | `TX{channel}{message}` or `TXM{channel}{seq}/{total}~{chunk}` | ASCII text string (auto-stored in RTC RAM inbox) |
| TST | `test{n}` (via sendTxtMessage) | Counter-incremented test string |
| RANGE | `RN{channel}test{n}` | Position + distance data (`isRangeMessage()` check) |
| PONG | `Ping!` | Static string, triggers pong response loop |
| PTT | `{P}{channel_bitrate_idx_raw audio_bytes}` | Phone-encoded Opus frames relayed via BLE ↔ LoRa |
| RAW | Passthrough | All received bytes displayed as hex if non-printable |
| SCAN | N/A | Measures RSSI/SNR only, no custom packets |
| WP | `WP~LA{lat},{lon}~AL{alt}~LB{label}~DI{id}` | Waypoint: lat/lng/altitude + 24 byte label, broadcast every 3s for 60s |


### Button behavior (all modes)

| Button | Hold Duration | Effect |
|---|---|---|
| MODE (P1.10) — Single click | <500ms | Cycle to next mode (wraps through all 9) |
| MODE (P1.10) — Double click | Two clicks <500ms apart | Next spreading factor → reinitializes LoRa |
| MODE (P1.10) — Hold | 5–10 seconds | Enter/exit settings mode |
| MODE (P1.10) — Hold | >10 seconds | Power off → shuts down peripherals, System OFF |

**Pin separation note:** P0.11 and P1.11 are **different pins**. `Touch_Pin = P0.11` (capacitive touch pad on the PCB), `ePaper_Backlight = P1.11` (e-paper LED backlight OUTPUT). They do not share a pin — unlike some reference examples (Display_lilygo.ino) where they used P0.11 for both backlight and touch, requiring mode reconfiguration.

### Pin definitions (from utilities.h)

- MODE_PIN = P1.10 (Button 2, red button on T-Echo)
- TOUCH_PIN = P0.11 (capacitive touch pad — INPUT_PULLUP at boot in `setupAppModes()`)
- ePaper_Backlight = P1.11 (OUTPUT, driven HIGH by display.cpp to power the e-paper LED)

### Long-press timer on MODE (P1.10)

`app_modes.cpp:182-197` uses a state machine tracking `holdDuration = millis() - btnPressTime` where `btnPressTime` is set when MODE_PIN goes LOW:
- ≥500ms and <10s → toggles settings mode (`toggleSettingsMode()` at line 193)
- ≥10s → powers off (`powerOff()` at line 196)

### Touch input (P0.11)

The touch pad (P0.11) can be tapped to change setting values while in settings mode — `updateCurrentSetting()` at `app_modes.cpp:404` detects falling edge (press) via `digitalRead(TOUCH_PIN)`. In non-settings modes, tap behavior is mode-specific: resets TST counters (line 265), syncs RAW counter (line 297), triggers PONG ping (line 303), toggles RANGE role (line 320).

### Radio (`lora`)

SX1262 via RadioLib. Non-blocking TX/RX queues. Spread factor adjustable via double-click. Packet counter synchronization across devices.

### Audio (`audio`)

Audio is **phone-only** — the T-Echo has no onboard mic/speaker. The device's role in PTT mode is to relay Opus frames: phone captures audio → encodes Opus → sends via BLE GATT → T-Echo transmits over LoRa; and vice versa for received audio.

### BLE (`ble`)

GATT service for companion app (Cordova/Android). Device scans as `LilygoT-Echo-XXXXXXXX`. GATT service UUID: `"1235"`, characteristic UUID: `"ABCE"` (simple string identifiers, not 128-bit standard UUIDs). Sends mode commands (`switchMode()`) and text messages (`"SENDTXT:{message}"`).

### BLE notification transport

All text notifications from firmware are wrapped with a `LINE:NOTIF|DATA:` prefix and `~~` terminators before being queued. The characteristic max length is 253 bytes; the notification string buffer must fit this full payload. **Bug fix (2026-07-02):** `notifStr` was only 130 bytes, truncating screen sync payloads up to 200 bytes + prefix (~220 chars total). Fixed by using a static buffer of `16 + 200 + 2 + 4 = 222` bytes. **Legacy mechanism removed (2026-07-30):** `updDisp()` no longer sends `LINE:NN|TEXT:` entries for each displayed line — these were queued during boot and never cleared, causing stale "Booting..." text to appear in the app before screen sync data arrived. Screen state is now communicated exclusively via the `LINE:S|M:...` forced screen sync payload.

### Screen sync per-mode content fields

Each mode adds specific key=value pairs between `C:` and `|S:` in the payload:

| Mode | Content fields sent |
|---|---|
| BEACON | `peer_name:{name}`, `peer_dist:{meters}m`, `roster_count:{n}`, `beacon_peer_alive:{0|1}`, `r{n}_n={name}`, `r{n}_d={dist}`, `r{n}_b={pct}%` (up to 5 roster rows) |
| RANGE | `range_role:{Sender|Receiver}`, `range_last_count:{n}`, `range_consecutive_ok:{n}`, `range_total_loss:{n}`, `range_stable_dist:{m}m`, `range_max_dist:{m}m`, `home_lat:{d}`, `home_lon:{d}`, `curr_lat:{d}`, `curr_lon:{d}` (if GPS locked) |
| PTT | `ptt_state:{TX|RX|Idle}` |
| TXT | `txt_inbox_count:{n}`, `txt_show_inbox:{0|1}`, `txt_scroll_page:{n}`, `txt_lora_alive:{0|1}`, `txt_latest_msg={text}` (if inbox closed and messages exist) |
| TST | `tst_sent:{n}`, `tst_recv:{n}` |
| PONG | `pong_state:{n}`, `pong_rtt_ms:{ms}` |
| SCAN | `s{pct}@{freq},{ch1_f},{ch1_r},...` (compact scan format, 5 top channels) |
| RAW | `raw_count:{n}` |
| WP | `wp_label={label}`, `wp_lat:{d}`, `wp_lon:{d}`, `wp_alt:{m}m`, `wp_broadcasting:{0|1}`, `wp_bcast_rem:{s}s` |

Status bar always adds: `S:{freq}|T:{hh:mm}|G:{sats}|B:{batt}%|I:{gpst=val,bat=idx}`. Icon values: `gpst=ok` or `gpst=no%`, `bat=0..6`.

### Panel refresh (settings mode)

The settings panel includes a "Panel Refresh" option that enters an interactive cycling mode. When activated, it runs up to 120 seconds of full white/black screen cycles (~8s each). While cycling, pressing the touch pad (P0.11) exits immediately. The display shows "Cycling..." in the settings value field during operation. Implemented by inlining the loop directly in `settings.cpp` rather than calling `panelRefreshToggle()` — the old function was removed and replaced with an inline `while` loop at `settings.cpp:240-275`.

## Crash Debug Infrastructure (`crash_debug.h`)

Installed crash diagnostics that capture state on every fault:

| Feature | Details |
|---|---|
| HardFault handler | Records full register dump, CFSR/HFSR/BFAR to RTC RAM (0x20007FC0) |
| Stack guard | Writes `0xDEADBEEF` to top of RAM (0x20007FFC); checks periodically in loop() |
| Debug log buffer | Circular 32-entry ring at 0x20006FC0 — use `dbgLog("fmt", args)` |
| Heap tracker | Tracks minimum free heap seen during operation |
| Peripheral crash detection | Identifies SPIM2 (display), SPIM3 (LoRa), TWIM1 (I2C/RTC) regions in BFAR |

Usage: call `dbgLog("[mod] msg")` in critical paths. On crash, full register dump + stack guard check + heap stats printed on next boot.


## Verification

1. **Build:** Run `build_scripts\01_build_firmware.bat` — must produce zero errors on release build (crash_debug adds ~2KB). If it fails, fix the code and retry until clean.
2. **Upload:** Run `build_scripts\02_upload_firmware.bat` with a **5-minute (300s) timeout** — ensures T-Echo enters DFU mode and receives the binary.
3. **Validate output:** Confirm build shows zero errors and upload completes without error/timeout. If upload times out or fails, do not mark the task as complete.
4. No automated tests exist; manual device testing is the only verification path beyond build/upload.

## Child DOX Index

None — this directory is managed by parent `AGENTS.md`.
