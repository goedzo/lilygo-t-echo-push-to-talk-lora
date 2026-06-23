# Agent Instructions — LoRa PTT Communication Project

## Architecture at a glance

Three codebases live in this repo:

| Directory | What it is |
|---|---|
| `main/` | **Firmware** for LilyGO T-Echo (nRF52840). Arduino sketch (.ino + .cpp/.h files). Compiles via Arduino IDE or PlatformIO (see build instructions below). |
| `cordova_app/PTTLora/` | Android companion app built with **Cordova** (`cordova-android 13`). BLE plugin only. Build: `02_build_project.bat` (Windows) or `cordova build android`. Output APK copied to `cordova_app/pttlora.apk`. |
| `lilygo_lora32_keyboard_bridge/main/` | Separate **bridge firmware** for a LilyGO LoRa32 board — relays BLE ↔ LoRa. Independent from the main firmware, but uses similar patterns. |
| `libraries/` | Vendored Arduino libraries (Adafruit, RadioLib, GxEPD2, Codec2, MCCI_LoRaWAN_LMIC, etc.). Must be copied to Arduino's `libraries` directory when building outside this repo. |

## Firmware build / flash

### Arduino CLI (primary — full automation)

Used for all CI/CD, command-line builds, and deployment. Verified working: **zero errors, 27% flash, 8% RAM**.

```bash
# Build
arduino-cli compile -b adafruit:nrf52:feather52840 --build-path .pio/t-echo-build main

# Upload (T-Echo in DFU mode)
arduino-cli upload -b adafruit:nrf52:feather52840 --port auto .pio/t-echo-build/main.bin

# Install dependencies
arduino-cli core install adafruit:nrf52@1.7.0
arduino-cli lib install "RadioLib GxEPD2 AceButton Codec2 TinyGPSPlus"
```

Windows batch scripts in `build_scripts/`:
| Script | Purpose |
|---|---|
| `01_build_firmware.bat` | Full build with dependency check |
| `02_upload_firmware.bat` | DFU upload to T-Echo |
| `03_ci_pipeline.bat` | Build > Verify > Flash pipeline |

**Prerequisites**: Arduino CLI is bundled with Arduino IDE (path: `D:\Tools\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe`) or standalone. Adafruit nRF52 core 1.7.0 installed via `core update-index`.

### PlatformIO / VSCode (NOT recommended)

The project has a `platformio.ini` at the repo root but **BLE compilation fails** due to a known incompatibility between PlatformIO's Nordic nRF52 v10+ framework and Bluefruit52Lib. Use Arduino CLI instead for automation.

### Build gotchas (Arduino CLI)

- The display is a **GxDEPG0150BN** 1.54" e-paper (not GxEPD2 stock). `display.cpp` includes a vendored header from `epd/GxEPD2_150_BN.h`. If this file changes, update both `main.ino` and `display.cpp` accordingly.
- Pin definitions differ between **VERSION_1** (commented out in `utilities.h`) and the default revision. Check which hardware revision you have before changing pin assignments.
- **DFU upload**: Double-click the T-Echo reset button to enter DFU mode, then upload via USB (`arduino-cli upload -b adafruit:nrf52:feather52840 --port auto main.bin`).
- The bootloader is Adafruit_nRF52_Arduino's default. Using nRF5-SDK will overwrite it — do not mix toolchains without restoring the bootloader first.

### PlatformIO configuration details (reference only)

| Setting | Value |
|---|---|
| Platform | `nordicnrf52@8.0.1` |
| Board | `adafruit_feather_nrf52840` |
| Framework | `arduino` |
| Upload | `nrfutil` (DFU) |
| Monitor speed | `115200` |

Libraries linked via `-I` include paths to `libraries/`. Source filter includes `main/lib/src` for RadioLib.

### PlatformIO environments

| Environment | Purpose |
|---|---|
| `t-echo` | Release build (optimized) |
| `t-echo-debug` | Debug build (`-O0 -g3`) |

## Firmware codebase entrypoints

- `main/main.ino` — setup/loop, board init, mode switch entry points
- `main/app_modes.cpp` — core mode logic (PTT, TXT, RAW, TST), button handling via AceButton/Button2
- `main/lora.cpp` — SX1262 radio config with RadioLib, non-blocking transmit/receive queues
- `main/display.cpp` — e-paper rendering (GxEPD2), margin/font constants defined at top of file
- `main/audio.cpp` — I2S capture/playback + Codec2 encode/decode
- `main/settings.cpp` — DeviceSettings struct persisted to RTC (PCF8563)
- `main/ble.cpp` — BLE GATT service for companion app communication

## Companion app build steps

```bash
cd cordova_app
# Create fresh project (skip if PTTLora already exists):
01_create_project.bat
# Build:
02_build_project.bat
```

The app scans for `LilygoT-Echo-XXXXXXXX` BLE devices and sends/receives LoRa messages.

## Libraries to watch

Key vendored libraries in `libraries/`:
- **RadioLib** — LoRa radio driver (SX1262)
- **GxEPD2 + GxEPD** — E-paper display drivers
- **Codec2** — Audio codec for PTT mode
- **Adafruit_nRF52** (via Board Manager, not vendored) — nRF52840 core
- **MCCI_LoRaWAN_LMIC_library** — LoRaWAN stack (may be used in future)

## No test harness

This repo has **no automated tests**. Verification is done by:
1. Compiling firmware in Arduino IDE or PlatformIO
2. Flashing to physical T-Echo hardware
3. Testing BLE connection from the companion APK

## Existing conventions

- `#pragma once` for header guards; no include-style guards
- CamelCase function names, camelCase variables (mixed with some uppercase defines)
- Global state is declared as `extern` in headers, defined in .cpp files
- The firmware uses `SerialMon` (alias for `Serial`) for debug output at 115200 baud
- `updDisp()` calls throughout firmware update the e-paper display; they are display-layer helpers


# DOX framework

- DOX is highly performant AGENTS.md hierarchy installed here
- Agent must follow DOX instructions across any edits

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

## Closeout

1. Re-check changed paths against the DOX chain
2. Update nearest owning docs and any affected parents or children
3. Refresh every affected Child DOX Index
4. Remove stale or contradictory text
5. Run existing verification when relevant
6. Report any docs intentionally left unchanged and why

## User Preferences

When the user requests a durable behavior change, record it here or in the relevant child AGENTS.md

## Child DOX Index

| Path | Scope |
|---|---|
| `main/AGENTS.md` | T-Echo firmware (nRF52840, SX1262, BLE, PTT/TXT/RAW/TST modes) |
| `build_scripts/AGENTS.md` | Arduino CLI build/upload/CI scripts (primary automation path) |
| `cordova_app/AGENTS.md` | Android companion app (Cordova + BLE plugin) |
| `lilygo_lora32_keyboard_bridge/AGENTS.md` | Bridge firmware (ESP32 LoRa32 ↔ BLE relay) |
| `libraries/AGENTS.md` | Vendored Arduino libraries (RadioLib, GxEPD2, Codec2, TinyGPSPlus, etc.) |