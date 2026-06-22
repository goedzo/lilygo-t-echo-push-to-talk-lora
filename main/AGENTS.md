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

## Local Contracts

- Board: `Nordic nRF52840 (PCA10056)`, target `adafruit_feather_nrf52840_s2`
- Compile via Arduino IDE or PlatformIO (no platformio.ini — create one per project)
- Debug output: `SerialMon` at 115200 baud
- Display updates: call `updDisp()` from app_modes or other modules that change state
- Header guards: `#pragma once`

## Build Gotchas

- **VERSION_1** pin definitions are commented out in `utilities.h`. Check hardware revision before editing pins.
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
GATT service for companion app (Cordova/Android). Device scans as `LilygoT-Echo-XXXXXXXX`.

## Verification

1. Verify in Arduino IDE — must compile clean (no warnings from pin defines or missing includes)
2. Flash to physical T-Echo hardware
3. Test BLE connection from `pttlora.apk`
4. No automated tests exist; manual device testing is the only verification path

## Child DOX Index

None yet. Create when a subfolder becomes a durable boundary with its own rules or workflow.
