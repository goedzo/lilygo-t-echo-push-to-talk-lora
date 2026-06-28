# Agent Instructions — LoRa PTT Communication Project

## Architecture at a glance

| Directory | What it is |
|---|---|
| `main/` | **Firmware** for LilyGO T-Echo (nRF52840). Arduino sketch (.ino + .cpp/.h files). Compiles via Arduino CLI. 16 source modules (+ crash_debug.h, utilities.h). Audio is phone-only — device relays Opus frames via BLE ↔ LoRa. 9 modes: BEACON, RAW, TXT, RANGE, TST, PONG, SCAN, PTT, WP. |
| `cordova_app/PTTLora/` | Android companion app built with **Cordova** (`cordova-android 13`). BLE plugin only. Build: `02_build_project.bat`. Output APK at `cordova_app/pttlora.apk`. GATT service UUID `"1235"`, char UUID `"ABCE"`. |
| `lilygo_lora32_keyboard_bridge/main/` | Separate **bridge firmware** for a LilyGO LoRa32 board (ESP32-based) — relays BLE ↔ LoRa. Independent from the main firmware, but uses similar patterns. |
| `libraries/` | Vendored Arduino libraries. 19 libs on disk (RadioLib, GxEPD2, Codec2, TinyGPSPlus, Adafruit-GFX-Library, etc.). Must be copied to Arduino's `libraries/` directory when building outside this repo. See `libraries/AGENTS.md` for full inventory. |
| `build_scripts/` | Arduino CLI automation: `01_build_firmware.bat`, `02_upload_firmware.bat`, `03_ci_pipeline.bat`. |

## Firmware build / flash

### Arduino CLI (primary — only working path)

```bash
# Build
arduino-cli compile -b adafruit:nrf52:feather52840 --build-path .pio/t-echo-build main

# Upload (T-Echo in DFU mode)
arduino-cli upload -b adafruit:nrf52:feather52840 --port auto .pio/t-echo-build/main.bin

# Install dependencies
arduino-cli core install adafruit:nrf52@1.7.0
arduino-cli lib install "RadioLib GxEPD2 AceButton TinyGPSPlus"
```

Verified: **~29% flash, ~16% RAM** (release build including crash_debug + text inbox + waypoints).

**Prerequisites**: Arduino CLI at `D:\Tools\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe` or in PATH. Adafruit nRF52 core 1.7.0 via `core update-index`.

### PlatformIO — NOT functional

The project has a stale `platformio.ini` reference but **PlatformIO cannot build this firmware**. BLE compilation fails due to Nordic nRF52 v10+ framework / Bluefruit52Lib incompatibility. Use Arduino CLI only.

### Build gotchas

- Display is **GxDEPG0150BN** 1.54" e-paper (not GxEPD2 stock). Vendored header: `epd/GxEPD2_150_BN.h`.
- Pin definitions differ between **VERSION_1** (commented out in `utilities.h`) and the default revision. Active pins: ePaper_Miso=P1.6, LoRa_Dio0=P0.22, GreenLed=P1.1, RedLed=P1.3, BlueLed=P0.14.
- **DFU upload**: Double-click T-Echo reset button to enter DFU mode, then `arduino-cli upload -b adafruit:nrf52:feather52840 --port auto main.bin`.
- nRF5-SDK overwrites Adafruit bootloader — do not mix toolchains without restoring bootloader first.

## Firmware codebase entrypoints

| File | Purpose |
|---|---|
| `main/main.ino` | setup/loop, board init, mode switch entry points |
| `main/app_modes.cpp/.h` | Core mode logic (9 modes), AceButton handling, waypoint save/broadcast state |
| `main/lora.cpp/.h` | SX1262 radio config with RadioLib, non-blocking TX/RX queues |
| `main/display.cpp/.h` | E-paper rendering (GxEPD2 for GxDEPG0150BN) |
| `main/settings.cpp/.h` | DeviceSettings struct persisted to RTC via PCF8563 |
| `main/ble.cpp/.h` | BLE GATT service for companion app (`"1235"` / `"ABCE"`) |
| `main/battery.cpp/.h` | Battery monitoring |
| `main/gps.cpp/.h` | GPS parsing (TinyGPSPlus) |
| `main/packet.cpp/.h` | Packet framing by mode (BEACON, TXT, TST, RANGE, PONG, PTT, RAW, SCAN, WP) |
| `main/text_inbox.cpp/.h` | Scrollable message inbox in RTC RAM (8 messages × 256 bytes, 0x200075C0) |
| `main/scan.cpp/.h` | Frequency scanner / OTA |
| `main/crash_debug.h` | HardFault recorder, stack guard, debug log buffer, heap tracker |
| `main/utilities.h` | Pin definitions (VERSION_1 commented out) |

## Companion app build steps

```bash
cd cordova_app
01_create_project.bat    # one-time only; skip if PTTLora/ already exists
02_build_project.bat     # produces pttlora.apk
```

The app scans for `LilygoT-Echo-XXXXXXXX` BLE devices and sends/receives LoRa messages over GATT.

## Libraries to watch (19 on disk)

RadioLib, GxEPD2, GxEPD, TinyGPSPlus, Adafruit_EPD, Adafruit-GFX-Library, Adafruit_BusIO, Adafruit_Sensor, MPU9250-0.4.6, PCF8563_Library, AceButton, Button2, SdFat - Adafruit Fork, SoftSPI, Adafruit SPIFlash, Adafruit_BME280_Library, ICM20948_WE, SensorLib.

Full inventory: `libraries/AGENTS.md`. Do not edit vendored libraries — fork upstream or vendor a patched copy.

## No automated tests

Verification path: 1) Build firmware via Arduino CLI (zero errors) → 2) Flash to T-Echo → 3) Test BLE + LoRa on hardware.

## Existing conventions

- `#pragma once` header guards; CamelCase functions, camelCase variables
- Global state: `extern` in headers, defined in .cpp files
- Debug output: `SerialMon` at 115200 baud
- Display updates via `updDisp()` calls from app_modes or other modules that change state

# DOX framework

## Core Contract

- AGENTS.md files are binding work contracts for their subtrees
- Work products, source materials, instructions, records, assets, and durable docs must stay understandable from the nearest applicable AGENTS.md plus every parent AGENTS.md above it

## Read Before Editing

1. Read the root AGENTS.md
2. Identify every file or folder you expect to touch
3. Walk from the repository root to each target path
4. Read every AGENTS.md found along each route
5. If a parent AGENTS.md lists a child AGENTS.md whose scope contains the path, read that child and continue from there
6. Use the nearest AGENTS.md as the local contract and parent docs for repo-wide rules
7. If docs conflict, the closer doc controls local work details, but no child doc may weaken DOX

Do not rely on memory. Re-read the applicable DOX chain in the current session before editing.

## Update After Editing

Every meaningful change requires a DOX pass before the task is done.

Update the closest owning AGENTS.md when a change affects:

- purpose, scope, ownership, or responsibilities
- durable structure, contracts, workflows, or operating rules
- required inputs, outputs, permissions, constraints, side effects, or artifacts
- user preferences about behavior, communication, process, organization, or quality
- AGENTS.md creation, deletion, move, rename, or index contents

Update parent docs when parent-level structure, ownership, workflow, or child index changes. Update child docs when parent changes alter local rules. Remove stale or contradictory text immediately. Small edits that do not change behavior or contracts may leave docs unchanged, but the DOX pass still must happen.

## Hierarchy

- Root AGENTS.md is the DOX rail: project-wide instructions, global preferences, durable workflow rules, and the top-level Child DOX Index
- Child AGENTS.md files own domain-specific instructions and their own Child DOX Index
- Each parent explains what its direct children cover and what stays owned by the parent
- The closer a doc is to the work, the more specific and practical it must be

## Child Doc Shape

- Create a child AGENTS.md when a folder becomes a durable boundary with its own purpose, rules, responsibilities, workflow, materials, or quality standards
- Work Guidance must reflect the current standards of the project or user instructions; if there are no specific standards or instructions yet, leave it empty
- Verification must reflect an existing check; if no verification framework exists yet, leave it empty and update it when one exists

Default section order:
- Purpose
- Ownership
- Local Contracts
- Work Guidance
- Verification
- Child DOX Index

## Style

- Keep docs concise, current, and operational
- Document stable contracts, not diary entries
- Put broad rules in parent docs and concrete details in child docs
- Prefer direct bullets with explicit names
- Do not duplicate rules across many files unless each scope needs a local version
- Delete stale notes instead of explaining history
- Trim obvious statements, repeated rules, misplaced detail, and warnings for risks that no longer exist

## Closeout — Mandatory Firmware Build Validation

Every code change to **any** firmware file requires the following build + upload validation before the task is considered done:

1. Re-check changed paths against the DOX chain
2. Update nearest owning docs and any affected parents or children
3. Refresh every affected Child DOX Index
4. Remove stale or contradictory text
5. Run existing verification when relevant
6. Report any docs intentionally left unchanged and why

### Firmware Build Validation (mandatory for firmware changes)

If **any** file in `main/` was added, modified, deleted, or renamed:

1. **Build:** Run `build_scripts\01_build_firmware.bat`
   - The build must produce **zero errors**. If it fails, fix the code and retry until clean.
2. **Upload:** If the build succeeds, run `build_scripts\02_upload_firmware.bat` with a **5-minute (300s) timeout**
   - Ensures the T-Echo enters DFU mode and the binary flashes without hanging.
3. **Validate output:** Confirm both steps completed successfully:
   - Build output shows zero errors and reports flash/RAM usage (~30% / ~8%)
   - Upload completes without error (device receives the new firmware)
   - If upload times out or fails, note the failure — do **not** mark the task as complete.

This validation applies regardless of which specific firmware module was changed (lora, audio, display, ble, app_modes, settings, etc.).

## User Preferences

When the user requests a durable behavior change, record it here or in the relevant child AGENTS.md

## Child DOX Index

| Path | Scope |
|---|---|
| `main/AGENTS.md` | T-Echo firmware (nRF52840, SX1262, BLE, 9 modes: BEACON, RAW, TXT, RANGE, TST, PONG, SCAN, PTT, WP) — 16 source modules + crash_debug.h |
| `build_scripts/AGENTS.md` | Arduino CLI build/upload/CI scripts (primary automation path; PlatformIO not functional) |
| `cordova_app/AGENTS.md` | Android companion app (Cordova android 13 + BLE plugin, GATT `"1235"` / `"ABCE"`) |
| `lilygo_lora32_keyboard_bridge/AGENTS.md` | Bridge firmware (ESP32 LoRa32 ↔ BLE relay) — independent codebase |
| `libraries/AGENTS.md` | Vendored Arduino libraries — 19 libs on disk, full inventory in that doc |
