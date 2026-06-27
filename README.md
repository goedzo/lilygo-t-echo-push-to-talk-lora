# LoRa PTT Communication Project with NRF52840 on T-Echo

## Overview

This project implements a Push-to-Talk (PTT) walkie-talkie system using LoRa communication on a LilyGO T-Echo board with an nRF52840 microcontroller. The device supports **7 operating modes** via single/double/long-press button combinations, with an e-paper display for status and message rendering.

## Modes

| Mode | Description |
|---|---|
| **RAW** | Raw packet dump — displays SNR, RSSI, receive count, and raw content line-by-line |
| **TXT** | Text messages — send/receive ASCII text (input via BLE companion app; on-device keyboard not yet implemented) |
| **RANGE** | Distance testing — sender broadcasts GPS position every 5s; receiver calculates stable/max range and packet loss |
| **TST** | Test beeps — auto-sends "test{n}" every 5s non-blocking; touch resets counters; displays SNR/RSSI/receive count |
| **PONG** | Manual ping-pong — touch sends "Ping!" with countdown; responds to incoming PING with own ping after delay |
| **SCAN** | Frequency scanner — scans 863–869.65 MHz in 0.01 MHz steps, ranks top 10 channels by quality score (RSSI+SNR) |
| **PTT** | Push-to-talk — hold touch to capture audio → Codec2 encode → transmit. **Note: I2S audio is stubbed** (T-Echo has no onboard microphone/speaker). PTT packet framing exists but requires external codec hardware (e.g., PCM1270). |

## Features

- **LoRa Communication**: Long-range, low-power communication between devices using the SX1262 LoRa module.
- **PTT Mode**: Push-to-talk voice mode with Codec2 audio encoding. **I2S audio capture/playback is currently stubbed** (no onboard microphone/speaker on T-Echo); PTT packet framing exists for external codec hardware.
- **TXT Mode**: Send/receive text messages over LoRa (input via BLE companion app).
- **RAW Mode**: Displays raw data packets received over LoRa with SNR, RSSI, and receive count.
- **TST Mode**: Auto-sends periodic test messages ("test{n}") every 5 seconds for signal testing.
- **RANGE Mode**: Distance testing with GPS — sender/receiver roles track stable range, max range, and packet loss.
- **PONG Mode**: Manual ping-pong for latency testing (touch to send "Ping!").
- **SCAN Mode**: Frequency scanner that ranks the top 10 channels by quality score.
- **E-Paper Display**: GxDEPG0150BN 1.54" e-paper display showing current mode, messages, and status.
- **Power-Off Function**: Hold the action button for 5 seconds to power off the device.
- **Double-Click Spreading Factor**: Quickly adjust the LoRa spreading factor (SF) using double-click on MODE button.
- **Battery Support**: Includes monitoring and display of battery status.
- **Non-Blocking Send & Receive**: Non-blocking packet transmission and reception queues.
- **Packet Counter Synchronization**: Keeps track of the number of packets sent and received.
- **Crash Diagnostics**: HardFault handler with register dump, stack guard, debug log buffer, and heap tracker (`main/crash_debug.h`).

## Codebase Structure

| Directory | What it is |
|---|---|
| `main/` | T-Echo firmware (nRF52840, SX1262, BLE, 7 modes) — Arduino sketch (.ino + .cpp/.h files) |
| `cordova_app/PTTLora/` | Android companion app (Cordova + BLE plugin). Build APK → `cordova_app/pttlora.apk` |
| `lilygo_lora32_keyboard_bridge/main/` | Bridge firmware (ESP32 LoRa32 ↔ BLE relay) — independent from main firmware |
| `libraries/` | Vendored Arduino libraries (19 libs) — copied to Arduino `libraries/` when building outside repo |
| `build_scripts/` | Arduino CLI automation: `01_build_firmware.bat`, `02_upload_firmware.bat`, `03_ci_pipeline.bat` |

## Hardware Requirements

- **Microcontroller**: LilyGO T-Echo with nRF52840
- **LoRa Module**: SX1262
- **E-Paper Display**: GxDEPG0150BN 1.54" e-paper (not stock GxEPD2)
- **Buttons**: MODE on P1.10, TOUCH on P0.11
- **Audio**: External codec hardware required for PTT (T-Echo has no onboard mic/speaker); PCM1270 via I2S recommended
- **Battery**: For mobile operation with battery status monitoring

## Firmware Build & Flash

### Arduino CLI (primary build tool)

```bash
# Build
arduino-cli compile -b adafruit:nrf52:feather52840 --build-path .pio/t-echo-build main

# Upload (T-Echo in DFU mode — double-click reset button first)
arduino-cli upload -b adafruit:nrf52:feather52840 --port auto .pio/t-echo-build/main.bin

# Install dependencies
arduino-cli core install adafruit:nrf52@1.7.0
arduino-cli lib install "RadioLib GxEPD2 AceButton Codec2 TinyGPSPlus"
```

Verified build output: **~30% flash, ~8% RAM** (release build with crash_debug).

### Prerequisites

- Arduino IDE installed at `D:\Tools\Arduino IDE` or standalone `arduino-cli.exe` in PATH
- Adafruit nRF52 core 1.7.0 (`arduino-cli core install adafruit:nrf52@1.7.0`)
- All vendored libraries from `libraries/` copied to Arduino's `libraries/` directory

### Build gotchas

- E-paper is **GxDEPG0150BN**. Vendored header: `epd/GxEPD2_150_BN.h`. Update both `main.ino` and `display.cpp` if this changes.
- DFU upload: double-click the T-Echo reset button to enter DFU mode before uploading.
- The bootloader is Adafruit_nRF52_Arduino's default. **nRF5-SDK overwrites it** — do not mix toolchains without restoring bootloader first.
- **PlatformIO is NOT functional** for this project. BLE compilation fails due to Bluefruit52Lib/SoftDevice SDK incompatibility with Nordic nRF52 v10+ framework.

## Code Overview

### Firmware entrypoints (`main/`)

| File | Purpose |
|---|---|
| `main.ino` | setup/loop, board init, mode switch entry points |
| `app_modes.cpp/.h` | Core mode logic (7 modes: RAW, TXT, RANGE, TST, PONG, SCAN, PTT), button handling via AceButton |
| `lora.cpp/.h` | SX1262 radio config with RadioLib, non-blocking TX/RX queues |
| `display.cpp/.h` | E-paper rendering (GxEPD2) for GxDEPG0150BN |
| `audio.cpp/.h` | I2S capture/playback + Codec2 encode/decode — **stubbed** (no onboard audio hardware) |
| `settings.cpp/.h` | DeviceSettings struct persisted to RTC via PCF8563 |
| `ble.cpp/.h` | BLE GATT service for companion app communication |
| `battery.cpp/.h` | Battery monitoring |
| `gps.cpp/.h` | GPS parsing (TinyGPSPlus) |
| `packet.cpp/.h` | Packet framing by mode |
| `scan.cpp/.h` | Frequency scanner / OTA |
| `crash_debug.h` | HardFault recorder, stack overflow guard, debug log buffer, heap tracker |
| `utilities.h` | Pin definitions (VERSION_1 is commented out; default revision active) |

### BLE companion app (`cordova_app/PTTLora/`)

- Cordova Android project with BLE plugin (`cordova-android 13`, `cordova-plugin-ble-central ^1.7.8`)
- Scans for `LilygoT-Echo-XXXXXXXX` MAC-prefixed BLE devices
- GATT service UUID: `"1235"`, characteristic UUID: `"ABCE"` (non-standard identifiers)
- Sends mode commands (`switchMode()`) and text messages (`"SENDTXT:{message}"`)
- Build: `01_create_project.bat` → `02_build_project.bat` → `cordova_app/pttlora.apk`

### Packet framing by mode

| Mode | Header / Format | Payload |
|---|---|---|
| TXT | `TX{channel}` | ASCII text string |
| TST | test{n} | Counter-incremented test string |
| RANGE | `RN{channel}` | Position + distance data (`isRangeMessage()` check) |
| PONG | Ping! | Static string, triggers pong response loop |
| PTT | `{P}{channel_bitrate_idx_raw audio_bytes}` | Codec2-encoded audio frames (audio stubbed) |
| RAW | Passthrough | All received bytes displayed as hex if non-printable |
| SCAN | N/A | Measures RSSI/SNR only, no custom packets |

### Button behavior (all modes)

| Button | Action | Effect |
|---|---|---|
| MODE (P1.10) — Single click | Cycle to next mode | Wraps around through all 7 |
| MODE — Double click | Next spreading factor | Reinitializes LoRa with new SF |
| MODE — Long press | Enter/exit settings mode | Cycles device settings |
| TOUCH (P0.11) — Hold >5s | Power off | Shuts down peripherals → System OFF via softdevice or NRF_POWER |

## Vendored Libraries (`libraries/`)

19 libraries on disk:

- **RadioLib** — LoRa radio driver (SX1262)
- **GxEPD2** + **GxEPD** — E-paper display drivers
- **Codec2** — Audio codec for PTT voice
- **TinyGPSPlus** — GPS parsing
- **Adafruit-EPD**, **Adafruit-GFX-Library**, **Adafruit_BusIO**, **Adafruit_Sensor** — Graphics/display stack
- **Adafruit_BME280_Library** — BME280 sensor driver
- **Adafruit SPIFlash** — SPI flash filesystem
- **MPU9250-0.4.6** — IMU support (versioned folder)
- **ICM20948_WE** — IMU sensor support
- **SensorLib** — General sensor library
- **PCF8563_Library** — RTC for settings persistence
- **AceButton**, **Button2** — Button handling
- **SdFat - Adafruit Fork** — SD card filesystem
- **SoftSPI** — Software SPI

> Libraries must be copied to Arduino's `libraries/` directory before building. Do not edit vendored libraries — fork upstream or vendor a patched copy.

## No Automated Tests

Verification is done by:
1. Compiling firmware (Arduino CLI)
2. Flashing to physical T-Echo hardware
3. Testing BLE connection from companion APK

## Troubleshooting

- **All libraries installed**: Ensure every folder in `libraries/` is present in Arduino's `libraries/` directory.
- **DFU mode**: If the board does not respond during upload, double-click the reset button to enter DFU mode and retry.
- **Serial debugging**: Check serial monitor at 115200 baud for `SerialMon` debug output.
- **Bootloader conflicts**: nRF5-SDK overwrites Adafruit bootloader — restore it before using Arduino toolchain again.
- **Flash variants**: T-Echo may ship with MX25R1635FZUIL0 or ZD25WQ16B flash memory chip.
- **NFC**: Not supported in Adafruit_nRF52_Arduino environment; use nRF5-SDK if NFC is required (will overwrite bootloader).

## Related Resources

- [LilyGO T-Echo](https://github.com/Xinyuan-LilyGO/LilyGo-T-Echo) — Hardware documentation
- [Adafruit nRF52 Arduino](https://github.com/adafruit/Adafruit_nRF52_Arduino) — Board package and libraries
- [Codec2](https://www.rowetel.com/?page_id=452) — Audio codec documentation
- [RadioLib](https://github.com/jgromes/RadioLib) — LoRa radio driver

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
