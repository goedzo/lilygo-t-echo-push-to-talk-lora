# LoRa PTT Communication Project with NRF52840 on T-Echo

## Overview

This project implements a Push-to-Talk (PTT) walkie-talkie system using LoRa communication on a LilyGO T-Echo board with an NRF52840 microcontroller. The device supports **7 operating modes** via single/double/long-press button combinations, with an e-paper display for status and message rendering.

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

- **LoRa Communication**: Enables long-range, low-power communication between devices using the SX1262 LoRa module.
- **PTT Mode**: Push-to-talk voice mode with Codec2 audio encoding. **I2S audio capture/playback is currently stubbed** (no onboard microphone/speaker on T-Echo); PTT packet framing exists for external codec hardware.
- **TXT Mode**: Send/receive text messages over LoRa (input via BLE companion app).
- **RAW Mode**: Displays raw data packets received over LoRa with SNR, RSSI, and receive count.
- **TST Mode**: Auto-sends periodic test messages ("test{n}") every 5 seconds for signal testing.
- **RANGE Mode**: Distance testing with GPS — sender/receiver roles track stable range, max range, and packet loss.
- **PONG Mode**: Manual ping-pong for latency testing (touch to send "Ping!").
- **SCAN Mode**: Frequency scanner that ranks the top 10 channels by quality score.
- **E-Paper Display**: Displays current mode, messages, and other relevant information on an e-paper display.
- **Power-Off Function**: Hold the action button for 5 seconds to power off the device.
- **Double-Click Action Button**: Quickly adjust the spreading factor (SF) using double-click. (Note: documented as "SPF" in some places, correct term is SF — Spreading Factor.)
- **Battery Support**: Includes monitoring and display of battery status.
- **Non-Blocking Send & Receive**: Improved performance through non-blocking packet transmission and reception.
- **Packet Counter Synchronization**: Keeps track of the number of packets sent and received, improving communication reliability.

## Hardware Requirements

- **Microcontroller**: LilyGO T-Echo with NRF52840
- **LoRa Module**: SX1262
- **E-Paper Display**: GxEPD2 1.54" e-paper display or similar
- **Buttons**: Two buttons (MODE on P1.10, TOUCH on P0.11) — mode switching, button actions per mode
- **Audio Components**: Codec2 library included but I2S hardware is stubbed (no onboard mic/speaker). External audio codec required for PTT functionality.
- **OTG Cable**: For connecting the microcontroller to an Android device (if using an Android development environment)
- **Battery**: For mobile operation, with support for battery status monitoring

## Software Requirements

- **Arduino IDE** (or **Arduino Studio** on Android)
- **Adafruit nRF52 Board Package**
- **Required Libraries**:
  - AceButton
  - Adafruit_BME280_Library
  - Adafruit_BusIO
  - Adafruit_EPD
  - Adafruit_GFX_Library
  - Adafruit_Sensor
  - Adafruit_Unified_Sensor
  - Button2
  - Codec2
  - GxEPD
  - GxEPD2
  - MCCI_LoRaWAN_LMIC_library
  - MPU9250
  - PCF8563_Library
  - RadioLib
  - SdFat_-_Adafruit_Fork
  - SerialFlash
  - SoftSPI
  - SoftSPIB
  - TinyGPSPlus

## Setup Instructions

### Using Arduino IDE

1. **Download and Install Arduino IDE**:
   - Download the latest version of the Arduino IDE from the [official website](https://www.arduino.cc/en/software).

2. **Configure Board Manager**:
   - Open Arduino IDE, then navigate to `File > Preferences`.
   - In the "Additional Boards Manager URLs" field, add the following URL:
     ```
     https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
     ```
   - Click "OK" to save the preferences.

3. **Install the Adafruit nRF52 Board Package**:
   - Go to `Tools > Board > Boards Manager`.
   - Search for "Adafruit nRF52" and click "Install" next to "Adafruit nRF52 by Adafruit".
   - After installation, select `Nordic nRF52840 (PCA10056)` from the board list.

4. **Copy Required Libraries**:
   - Copy all the folders from the `libraries` directory in this project to your Arduino libraries directory:
     ```
     C:\Users\<YourName>\Documents\Arduino\libraries
     ```
   - The required libraries include:
     - AceButton
     - Adafruit_BME280_Library
     - Adafruit_BusIO
     - Adafruit_EPD
     - Adafruit_GFX_Library
     - Adafruit_Sensor
     - Adafruit_Unified_Sensor
     - Button2
     - Codec2
     - GxEPD
     - GxEPD2
     - MCCI_LoRaWAN_LMIC_library
     - MPU9250
     - PCF8563_Library
     - RadioLib
     - SdFat_-_Adafruit_Fork
     - SerialFlash
     - SoftSPI
     - SoftSPIB
     - TinyGPSPlus
     - SPI.h (usually pre-installed with Arduino IDE)
     - Wire.h (usually pre-installed with Arduino IDE)

5. **Connect the T-Echo Board**:
   - Connect your T-Echo board to your computer via USB.
   - In the Arduino IDE, go to `Tools > Port` and select the port corresponding to your board.

6. **Upload the Code**:
   - Open the project sketch in Arduino IDE.
   - Click the upload button to compile and upload the code to your T-Echo board.

### Using PlatformIO

1. **Install VSCode and Python**:
   - Download and install [Visual Studio Code (VSCode)](https://code.visualstudio.com/) and [Python](https://www.python.org/downloads/).

2. **Install PlatformIO**:
   - Open VSCode and go to the Extensions view by clicking on the square icon on the left sidebar.
   - Search for "PlatformIO IDE" and install the extension.

3. **Open the Project**:
   - Open the T-Echo project folder in VSCode by going to `File > Open Folder`.

4. **Compile and Upload**:
   - On the PlatformIO home page, click the checkmark (√) in the lower left corner to compile the project.
   - Click the arrow (→) to upload the firmware to the T-Echo board.

5. **DFU Mode (for USB Upload)**:
   - If you are uploading firmware via USB (`upload_protocol = nrfutil`), double-click the reset button on the T-Echo to enter DFU mode before uploading.

### Precautions

- **Library Files**: Ensure that the files in the `libraries` directory are correctly placed in your Arduino libraries directory, as they include crucial dependencies.
- **Pin Compatibility**: The T-Echo pin assignments may not be directly compatible with the official SDK. Pay special attention to pin definitions if using the nRF5-SDK.
- **Bootloader**: The T-Echo comes with the Adafruit_nRF52_Arduino bootloader pre-installed. Programming the board with the nRF5-SDK will overwrite this bootloader.
- **NFC Functionality**: NFC is not supported in the Adafruit_nRF52_Arduino environment; use the nRF5-SDK if NFC functionality is required.
- **Flash Memory**: The T-Echo may use either the MX25R1635FZUIL0 or ZD25WQ16B flash memory chip. Be aware of the differences when programming.
- **Burning a New Bootloader**: If you need to restore the bootloader, refer to the official documentation on burning a new bootloader.

## Code Overview

### `main.cpp`
The main entry point of the program. Initializes all peripherals and handles the main loop, switching between modes.

### `display.cpp`
Manages the e-paper display, including rendering icons and text. Handles mode-specific display updates.

### `lora.cpp`
Configures and controls the LoRa communication module, including setting power and current limits.

### `audio.cpp`
**Audio capture/playback is disabled.** T-Echo has no onboard microphone or speaker. Both `capAudio()` and `playAudio()` are empty stubs. PTT mode packet framing exists (`PTT` type with Codec2-encoded frames) but cannot transmit or play audio without external codec hardware (e.g., PCM1270 via I2S).

### `settings.cpp`
Manages user settings, including mode switching and configuration adjustments. Refactored to store settings in a `DeviceSettings` struct.

### `app_modes.cpp`
Implements the core logic for all 7 operational modes (RAW, TXT, RANGE, TST, PONG, SCAN, PTT). Uses an array of mode strings and AceButton library for button handling. Includes `sendTestMessage()` (auto every 5s), `sendRangeMessage()`, `sendTxtMessage()`, `powerOff()` (System OFF via softdevice or NRF_POWER), and debounced touch press detection. Packet reception is routed per-mode: RAW/TST display SNR/RSSI/counters, TXT shows text, RANGE tracks distance/packet loss, PONG responds with ping loop, PTT decodes Codec2 audio. Audio I2S functions (`capAudio`, `playAudio`) are stubbed — PTT requires external hardware.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgements

- **Adafruit** for providing robust hardware and software libraries for the NRF52840.
- **RadioLib** for comprehensive LoRa communication support.
- **Codec2** for the open-source audio codec.
- **LilyGO** for the T-Echo hardware platform.

## Troubleshooting

If you encounter any issues:

- **Ensure all libraries are correctly installed and up to date**.
- **Verify wiring connections**, particularly for SPI and I2S peripherals.
- **Check the serial monitor for debugging messages** during setup and operation.
- **Consult the documentation** for the respective libraries and hardware modules.
- **DFU Mode**: If the board does not appear to respond when trying to upload new firmware, double-click the reset button to enter DFU mode and try uploading again.

For further assistance, feel free to open an issue in the repository or contact the project maintainers.

## Additional Notes

- **NFC Functionality**: The NFC feature is not supported when using the Adafruit_nRF52_Arduino environment. To use NFC, you must switch to using the nRF5-SDK. Be aware that doing so will overwrite the pre-installed bootloader on your T-Echo.
- **Flash Memory Variants**: The T-Echo board may come with either the MX25R1635FZUIL0 or ZD25WQ16B flash memory chip. Ensure you account for the specific chip variant in your code if flash memory is used.
- **Using nRF5-SDK**: If you intend to use the nRF5-SDK, remember to check the pin configurations and manage the bootloader accordingly. Instructions for burning a new bootloader can be found in the official T-Echo documentation.

## Related Resources

- **T-Echo Official Documentation**: Visit the [T-Echo GitHub repository](https://github.com/Xinyuan-LilyGO/LilyGo-T-Echo) for detailed hardware documentation and additional resources.
- **Adafruit nRF52 Libraries**: The [Adafruit GitHub repository](https://github.com/adafruit/Adafruit_nRF52_Arduino) contains the official libraries and tools for working with the nRF52 series.
- **Codec2 Documentation**: Learn more about the [Codec2 audio codec](https://www.rowetel.com/?page_id=452) used in this project.

## Contributing

Contributions are welcome! If you find a bug or want to add a new feature, feel free to open an issue or submit a pull request. Please ensure your code adheres to the project's coding standards.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
