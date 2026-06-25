# Agent Instructions — Firmware Build Scripts (Arduino CLI)

## Purpose

Automated build and upload scripts for the T-Echo firmware using Arduino CLI. Full CI/CD pipeline — no IDE required.

## Ownership

- `01_build_firmware.bat` — Full build with dependency check, size reporting
- `02_upload_firmware.bat` — DFU upload to T-Echo via `arduino-cli upload --port auto`
- `03_ci_pipeline.bat` — Build > Verify > Flash pipeline (CI-friendly)

## Local Contracts

- **Prerequisite:** Arduino IDE installed at `D:\Tools\Arduino IDE` or standalone `arduino-cli.exe` available in PATH
- **Board FQBN:** `adafruit:nrf52:feather52840` (Adafruit Feather nRF52840 Express)
- **Core version:** `adafruit:nrf52@1.7.0`
- **Build command:** `arduino-cli compile -b adafruit:nrf52:feather52840 --build-path .pio/t-echo-build main`
- **Upload command:** `arduino-cli upload -b adafruit:nrf52:feather52840 --port auto .pio/t-echo-build/main.bin`
- **RadioLib 7.1.2** — vendored in `libraries/` and bundled in `main/lib/`; copied to Arduino lib dir during build (not from Library Manager)
- Build output for nRF52 is `.elf`/`.hex` (not `.bin`)
- All vendored libraries are installed via `arduino-cli lib install "Library Name"` from Library Manager

## Known Limitations

- PlatformIO cannot build this firmware (Bluefruit52Lib/SoftDevice SDK incompatibility)
- Arduino CLI is the required build tool — it uses the same Adafruit nRF52 core as Arduino IDE

## Verification

1. Run `03_ci_pipeline.bat` — all checks must pass (core installed, libraries present, compilation successful)
2. Verified result: **zero errors, 27% flash, 8% RAM** on release build
