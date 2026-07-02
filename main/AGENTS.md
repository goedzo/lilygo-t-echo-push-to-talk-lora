# Agent Instructions — Main Firmware (LilyGO T-Echo)

## Purpose

Takes care of the T-Echo firmware (nRF52840, SX1262, BLE, 9 modes: BEACON, RAW, TXT, RANGE, TST, PONG, SCAN, PTT, WP) — 16 source modules + crash_debug.h. Audio is entirely handled by the phone app; the T-Echo relays Opus frames via BLE ↔ LoRa only.

## Ownership

- **Entry file:** `main/main.ino`
- **Mode logic:** `main/app_modes.cpp/.h`
- **Radio:** `main/lora.cpp/.h`
- **Display:** `main/display.cpp/.h` (GxDEPG0150BN 1.54" e-paper, GxEPD2)
- **Text Inbox:** `main/text_inbox.cpp/.h` — Scrollable message inbox stored in RTC RAM (8 messages × 256 bytes, address 0x200075C0) with sender/timestamp. 16-line scroll display on E-Paper. Auto-stored on incoming TXT/TXT_MULTI packets
- **BLE GATT:** `main/ble.cpp/.h` — relays Opus frames from phone → LoRa, and received LoRa audio → phone via BLE notify (binary)
- **Packet framing:** `main/packet.cpp/.h`
- **BLE GATT:** `main/ble.cpp/.h`
- **Settings (RTC):** `main/settings.cpp/.h` (PCF8563)
- **GPS:** `main/gps.cpp/.h` (TinyGPSPlus)
- **Battery:** `main/battery.cpp/.h`
- **Packet framing:** `main/packet.cpp/.h`
- **Scan/OTA:** `main/scan.cpp/.h` — Frequency scanner (top 10 channels by quality)
- **Pin definitions:** `main/utilities.h`
- **Crash detection & debugging:** `main/crash_debug.h` — HardFault recorder, stack overflow guard, debug log buffer, heap tracker

## Local Contracts

- Board: nRF52840 (LilyGO T-Echo PCA10056) targeting `adafruit:nrf52:feather52840`
- **Default revision is active** — VERSION_1 pins are commented out in `utilities.h`
- Active pins: ePaper_Miso=P1.6, LoRa_Dio0=P0.22, GreenLed_Pin=P1.1, RedLed_Pin=P1.3, BlueLed_Pin=P0.14
- Compile via **Arduino CLI only** — PlatformIO is not functional for this firmware
- Debug output: `SerialMon` at 115200 baud
- Display updates: call `updDisp()` from app_modes or other modules that change state
- Header guards: `#pragma once`

### Build instructions (Arduino CLI)

```bash
arduino-cli compile -b adafruit:nrf52:feather52840 --build-path .pio/t-echo-build main
arduino-cli upload -b adafruit:nrf52:feather52840 --port auto .pio/t-echo-build/main.bin
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

Seven modes: RAW (raw packet dump), TXT (text messages — input via BLE app; inbox with scrollable message history on-device), RANGE (distance testing with GPS sender/receiver roles), TST (auto test beeps every 5s), PONG (manual ping-pong), SCAN (frequency scanner, top 10 channels), PTT (push-to-talk — Opus frames relayed from phone via BLE GATT to LoRa), BEACON (peer roster — broadcasts GPS/battery beacon, displays list of detected peers with distances and battery levels), WP (waypoint — touch to save current GPS as waypoint packet, broadcast on LoRa for 60s). Button/AceButton handles double-click for SF adjustment and long-press for power-off. MODE click in TXT mode toggles inbox/single-message view.

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

| Button | Action | Effect |
|---|---|---|
| MODE (P1.9) — Single click | Cycle to next mode | Wraps around through all 9 |
| MODE — Double click | Next spreading factor | Reinitializes LoRa with new SF |
| MODE — Long press | Enter/exit settings mode | Cycles device settings |
| TOUCH (P0.11) — Hold >5s | Power off | Shuts down peripherals → System OFF via softdevice or NRF_POWER |

### Radio (`lora`)

SX1262 via RadioLib. Non-blocking TX/RX queues. Spread factor adjustable via double-click. Packet counter synchronization across devices.

### Audio (`audio`)

Audio is **phone-only** — the T-Echo has no onboard mic/speaker. The device's role in PTT mode is to relay Opus frames: phone captures audio → encodes Opus → sends via BLE GATT → T-Echo transmits over LoRa; and vice versa for received audio.

### BLE (`ble`)

GATT service for companion app (Cordova/Android). Device scans as `LilygoT-Echo-XXXXXXXX`. GATT service UUID: `"1235"`, characteristic UUID: `"ABCE"` (simple string identifiers, not 128-bit standard UUIDs). Sends mode commands (`switchMode()`) and text messages (`"SENDTXT:{message}"`).

### BLE notification transport

All text notifications from firmware are wrapped with a `LINE:NOTIF|DATA:` prefix and `~~` terminators before being queued. The characteristic max length is 253 bytes; the notification string buffer must fit this full payload. **Bug fix (2026-07-02):** `notifStr` was only 130 bytes, truncating screen sync payloads up to 200 bytes + prefix (~220 chars total). Fixed by using a static buffer of `16 + 200 + 2 + 4 = 222` bytes.

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

1. **Build:** Run `build_scripts\01_build_firmware.bat` — must produce zero errors, ~30% flash / ~8% RAM on release build (crash_debug adds ~2KB). If it fails, fix the code and retry until clean.
2. **Upload:** Run `build_scripts\02_upload_firmware.bat` with a **5-minute (300s) timeout** — ensures T-Echo enters DFU mode and receives the binary.
3. **Validate output:** Confirm build shows zero errors and upload completes without error/timeout. If upload times out or fails, do not mark the task as complete.
4. No automated tests exist; manual device testing is the only verification path beyond build/upload.

## Child DOX Index

None — this directory is managed by parent `AGENTS.md`.
