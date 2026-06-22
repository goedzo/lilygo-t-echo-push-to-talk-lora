# Agent Instructions — Bridge Firmware (LilyGO LoRa32)

## Purpose

Takes care of the independent bridge firmware: `lilygo_lora32_keyboard_bridge/main/`. A separate LilyGO LoRa32 board that relays BLE ↔ LoRa between BLE peripherals and LoRa networks. Independent from main firmware but shares similar coding patterns.

## Ownership

- **Entry file:** `main/main.ino`
- **BLE logic:** `main/ble.cpp/.h`, `main/bluetooth.cpp/.h`
- **Display:** `main/display.cpp/.h` (e-paper)
- **Utility/pins:** `main/utility.cpp/.h`, `main/utilities.h`, `main/globals.h`
- **Archive:** `main/main_old.cpp.old`

## Local Contracts

- Target: LilyGO LoRa32 board (ESP32-based, not nRF52)
- Independent from main firmware — no shared headers or code
- Follows similar BLE ↔ LoRa relay pattern but on different hardware platform

## Work Guidance

### Architecture
Bridge device connects BLE-capable devices to LoRa network. Acts as a gateway between the two radio domains. Does not share source code with `main/` firmware despite conceptual similarity.

## Verification

1. Compile in Arduino IDE targeting appropriate ESP32 board
2. Flash to physical LoRa32 hardware
3. Verify BLE ↔ LoRa relay functionality on device

## Child DOX Index

None yet. Create when subfolders acquire their own durable rules or workflows.
