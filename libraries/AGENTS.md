# Agent Instructions — Vendored Libraries

## Purpose

Takes care of all vendored Arduino libraries in `libraries/`. These third-party and forked libraries must be copied to the Arduino `libraries` directory when building outside this repo. This folder is not source-controlled for edits — libraries are vendored as-is from upstream or forks.

## Ownership

| Library | Purpose |
|---|---|
| `RadioLib` | LoRa radio driver (SX1262) |
| `GxEPD2` | E-paper display driver |
| `GxEPD` | Legacy e-paper support |
| `Codec2` | Audio codec for PTT voice |
| `TinyGPSPlus` | GPS parsing |
| `Adafruit_EPD` | Adafruit EPD driver |
| `Adafruit-GFX-Library` | Graphics core (folder uses hyphen) |
| `Adafruit_BusIO` | I2C/SPI abstraction |
| `Adafruit_Sensor` | Sensor abstraction |
| `MPU9250-0.4.6` | IMU support (versioned folder name) |
| `PCF8563_Library` | RTC (settings persistence) |
| `AceButton` | Button handling |
| `Button2` | Alternative button library |
| `SdFat - Adafruit Fork` | SD card filesystem (folder uses spaces) |
| `SoftSPI` | Software SPI (non-hardware) |
| `Adafruit SPIFlash` | SPI flash filesystem |
| `Adafruit_BME280_Library` | BME280 sensor driver |
| `ICM20948_WE` | IMU sensor support |
| `SensorLib` | General sensor library |

## Local Contracts

- **Do not edit vendored libraries.** If a library needs changes, fork it upstream or vendor a patched copy.
- Libraries must be copied to Arduino's `libraries` directory before building.
- Run instructions in `libraries/copy to arduino_libraries.txt` for setup.

## Work Guidance

### Adding a new library
1. Download the library archive from upstream (GitHub release or Arduino Library Manager)
2. Unzip into a new folder under `libraries/` with exact repo name
3. Verify it compiles cleanly with the firmware
4. Update this file's table

### Updating an existing library
1. Fetch latest version from upstream
2. Replace the folder contents (keep the folder name)
3. Verify firmware still compiles and functions correctly
4. Update this file's table if folder name or API changes

## Verification

- All listed libraries must compile without errors when present in Arduino's `libraries` directory
- Firmware (`main/`) must build clean with all libraries present
- No automated checks; compile-time verification is the only path

## Child DOX Index

None yet. Create when subfolders acquire their own durable rules or workflows.
