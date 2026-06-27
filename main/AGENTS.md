# Agent Instructions — Main Firmware (LilyGO T-Echo)

## Purpose

Takes care of the T-Echo firmware (nRF52840, SX1262, BLE, 7 modes: RAW, TXT, RANGE, TST, PONG, SCAN, PTT) — 14 source modules + crash_debug.h. Audio is entirely handled by the phone app; the T-Echo relays Opus frames via BLE ↔ LoRa only.

## Ownership

- **Entry file:** `main/main.ino`
- **Mode logic:** `main/app_modes.cpp/.h`
- **Radio:** `main/lora.cpp/.h`
- **Display:** `main/display.cpp/.h` (GxDEPG0150BN 1.54" e-paper, GxEPD2)
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

## Work Guidance

### Mode logic (`app_modes`)

Seven modes: RAW (raw packet dump), TXT (text messages — input via BLE app; on-device keyboard stubbed), RANGE (distance testing with GPS sender/receiver roles), TST (auto test beeps every 5s), PONG (manual ping-pong), SCAN (frequency scanner, top 10 channels), PTT (push-to-talk — Opus frames relayed from phone via BLE GATT to LoRa). Button/AceButton handles double-click for SF adjustment and long-press for power-off.

### Packet framing by mode

| Mode | Header | Payload |
|---|---|---|
| TXT | `TX{channel}{message}` | ASCII text string |
| TST | `test{n}` (via sendTxtMessage) | Counter-incremented test string |
| RANGE | `RN{channel}test{n}` | Position + distance data (`isRangeMessage()` check) |
| PONG | `Ping!` | Static string, triggers pong response loop |
| PTT | `{P}{channel_bitrate_idx_raw audio_bytes}` | Phone-encoded Opus frames relayed via BLE ↔ LoRa |
| RAW | Passthrough | All received bytes displayed as hex if non-printable |
| SCAN | N/A | Measures RSSI/SNR only, no custom packets |

### Button behavior (all modes)

| Button | Action | Effect |
|---|---|---|
| MODE (P1.10) — Single click | Cycle to next mode | Wraps around through all 7 |
| MODE — Double click | Next spreading factor | Reinitializes LoRa with new SF |
| MODE — Long press | Enter/exit settings mode | Cycles device settings |
| TOUCH (P0.11) — Hold >5s | Power off | Shuts down peripherals → System OFF via softdevice or NRF_POWER |

### Radio (`lora`)

SX1262 via RadioLib. Non-blocking TX/RX queues. Spread factor adjustable via double-click. Packet counter synchronization across devices.

### Audio (`audio`)

Audio is **phone-only** — the T-Echo has no onboard mic/speaker. The device's role in PTT mode is to relay Opus frames: phone captures audio → encodes Opus → sends via BLE GATT → T-Echo transmits over LoRa; and vice versa for received audio.

### BLE (`ble`)

GATT service for companion app (Cordova/Android). Device scans as `LilygoT-Echo-XXXXXXXX`. GATT service UUID: `"1235"`, characteristic UUID: `"ABCE"` (simple string identifiers, not 128-bit standard UUIDs). Sends mode commands (`switchMode()`) and text messages (`"SENDTXT:{message}"`).

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
