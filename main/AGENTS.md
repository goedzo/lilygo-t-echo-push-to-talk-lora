# Agent Instructions — Main Firmware (LilyGO T-Echo)

## Purpose

Takes care of the T-Echo firmware: `main/` directory. The core device that runs PTT, TXT, RAW, and TST modes on nRF52840 with SX1262 LoRa, e-paper display, BLE GATT, audio (Codec2), GPS, battery monitoring, and button input.

## Ownership

- **Entry file:** `main/main.ino`
- **Mode logic:** `main/app_modes.cpp/.h`
- **Radio:** `main/lora.cpp/.h`
- **Display:** `main/display.cpp/.h` (GxDEPG0150BN 1.54" e-paper, GxEPD2)
- **Audio:** `main/audio.cpp/.h` (I2S + Codec2)
- **BLE GATT:** `main/ble.cpp/.h`
- **Settings (RTC):** `main/settings.cpp/.h` (PCF8563)
- **GPS:** `main/gps.cpp/.h` (TinyGPSPlus)
- **Battery:** `main/battery.cpp/.h`
- **Packet framing:** `main/packet.cpp/.h`
- **Scan/OTA:** `main/scan.cpp/.h`
- **Pin definitions:** `main/utilities.h`
- **Crash detection & debugging:** `main/crash_debug.h` — HardFault recorder, stack overflow guard, debug log buffer, heap tracker

## Local Contracts

- Board: `Nordic nRF52840 (PCA10056)`, target `adafruit_feather_nrf52840_s2`
- **VERSION_1 pin definitions are commented out** in `utilities.h` — default revision is active with ePaper_Miso=P1.6, LoRa_Dio0=P0.22, GreenLed_Pin=P1.1, RedLed_Pin=P1.3, BlueLed_Pin=P0.14
- Compile via Arduino IDE or PlatformIO (see build instructions below)
- Debug output: `SerialMon` at 115200 baud
- Display updates: call `updDisp()` from app_modes or other modules that change state
- Header guards: `#pragma once`

### Build instructions

**Arduino CLI (primary — full automation):**
```bash
arduino-cli compile -b adafruit:nrf52:feather52840 --build-path .pio/t-echo-build main
arduino-cli upload -b adafruit:nrf52:feather52840 --port auto .pio/t-echo-build/main.bin
```

**Arduino IDE:**
1. Install `Adafruit nRF52` board package via Board Manager URL in Preferences
2. Select board: `Adafruit Feather nRF52840 Express`
3. Copy all folders from `libraries/` into your Arduino `libraries` directory
4. Open `main/main.ino`, verify, upload

**PlatformIO (NOT recommended):** BLE does not compile with PlatformIO's Nordic nRF52 v10+ due to Bluefruit52Lib/SDK config incompatibility. Use Arduino CLI instead.

The project includes a `platformio.ini` at the repo root with two environments: `t-echo` (release) and `t-echo-debug`. All vendored libraries are referenced via `-I` include paths. Build uses `build_src_filter` to also compile RadioLib source from `main/lib/src`.

**No platformio.ini found on disk** — documentation is stale. PlatformIO is not functional; use Arduino CLI.

## Build Gotchas

- E-paper is **GxDEPG0150BN**. The vendored header is `epd/GxEPD2_150_BN.h`. If it changes, update both `main.ino` and `display.cpp`.
- **DFU upload**: double-click reset button to enter DFU mode first.
- nRF5-SDK overwrites the Adafruit bootloader — do not mix toolchains without restoring bootloader.

## Work Guidance

### Mode logic (`app_modes`)
Four modes: PTT (voice), TXT (text messages), RAW (raw packet dump), TST (test beeps). Button2/AceButton handles double-click for SPF adjustment and long-press for power-off.

### Radio (`lora`)
SX1262 via RadioLib. Non-blocking TX/RX queues. Spread factor adjustable via double-click. Packet counter synchronization across devices.

### Audio (`audio`)
I2S capture → Codec2 encode → transmit. Receive → Codec2 decode → I2S playback.

### BLE (`ble`)
GATT service for companion app (Cordova/Android). Device scans as `LilygoT-Echo-XXXXXXXX`. BLE service UUID: `"1235"`, characteristic UUID: `"ABCE"` (simple string identifiers, not 128-bit standard UUIDs).

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

| Path | Scope |
|---|---|
| `build_scripts/` | Arduino CLI build/upload/CI scripts (`01_build_firmware.bat`, `02_upload_firmware.bat`, `03_ci_pipeline.bat`) |
