# LoRa PTT Communication Project with NRF52840

## Overview

This project implements a Push-to-Talk (PTT) walkie-talkie system using LoRa communication on an NRF52840 microcontroller. The project is designed to work with an e-paper display (e.g., GxEPD2 1.54" display) and supports different operating modes including PTT, text message display (TXT), raw data display (RAW), and a test mode (TST). 

## Features

- **LoRa Communication**: Enables long-range, low-power communication between devices using the SX1262 LoRa module.
- **PTT Mode**: Allows voice communication using a push-to-talk mechanism, with audio encoding/decoding using Codec2.
- **TXT Mode**: Displays text messages received over LoRa.
- **RAW Mode**: Displays raw data packets received over LoRa without filtering.
- **Test Mode**: Sends periodic test messages to verify communication.
- **E-Paper Display**: Displays current mode, messages, and other relevant information on an e-paper display.

## Hardware Requirements

- **Microcontroller**: NRF52840 (e.g., Adafruit Feather nRF52840, Nordic nRF52840 DK)
- **LoRa Module**: SX1262
- **E-Paper Display**: GxEPD2 1.54" e-paper display or similar
- **Buttons**: Two buttons for mode switching and PTT action
- **Audio Components**: Microphone and speaker (for voice communication)
- **OTG Cable**: For connecting the microcontroller to an Android device (if using an Android development environment)

## Software Requirements

- **Arduino IDE** (or **Arduino Studio** on Android)
- **Adafruit nRF52 Board Package**
- **GxEPD2 Library**: For controlling the e-paper display
- **RadioLib Library**: For LoRa communication
- **Codec2 Library**: For audio encoding/decoding
- **Adafruit GFX Library**: Required for display functionality

## Setup Instructions

1. **Install Arduino IDE/Arduino Studio**:
   - Download and install the Arduino IDE on your desktop, or Arduino Studio on your Android device.

2. **Install the Adafruit nRF52 Board Package**:
   - In the Arduino IDE, go to `File > Preferences`.
   - Add the following URL to the Additional Boards Manager URLs: `https://adafruit.github.io/arduino-board-index/package_adafruit_index.json`.
   - Go to `Tools > Board > Boards Manager`, search for "Adafruit nRF52", and install the package.

3. **Install Required Libraries**:
   - Use the Library Manager in Arduino IDE (`Sketch > Include Library > Manage Libraries...`) to install the following libraries:
     - `RadioLib`
     - `GxEPD2`
     - `Adafruit GFX`
     - `Codec2`

4. **Wiring Connections**:
   - **LoRa Module**:
     - Connect `MOSI`, `MISO`, `SCK`, `CS`, `RST`, and `IRQ` to the respective pins on the NRF52840.
   - **E-Paper Display**:
     - Connect `CS`, `DC`, `RST`, and `BUSY` pins to the NRF52840.
   - **Buttons**:
     - Connect one button to `GPIO` for PTT action and another for mode switching.
   - **Audio Components**:
     - Connect the microphone and speaker to appropriate I2S pins.

5. **Upload the Code**:
   - Open the provided code files in the Arduino IDE.
   - Select your board (`Adafruit Feather nRF52840` or similar).
   - Compile and upload the code to your NRF52840 board.

6. **Operating the Device**:
   - **PTT Mode**: Press and hold the PTT button to transmit audio.
   - **TXT Mode**: Display received text messages.
   - **RAW Mode**: Display raw LoRa packets.
   - **Test Mode**: Automatically send test messages every 10 seconds.

## Code Overview

### `main.cpp`
The main entry point of the program. Initializes all peripherals and handles the main loop, switching between modes.

### `display.cpp`
Manages the e-paper display, including rendering icons and text. Handles mode-specific display updates.

### `lora.cpp`
Configures and controls the LoRa communication module, including setting power and current limits.

### `audio.cpp`
Handles audio capture and playback using the NRF52840's I2S interface, with Codec2 encoding/decoding.

### `settings.cpp`
Manages user settings, including mode switching and configuration adjustments.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgements

- **Adafruit** for providing robust hardware and software libraries for the NRF52840.
- **RadioLib** for comprehensive LoRa communication support.
- **Codec2** for the open-source audio codec.

## Troubleshooting

If you encounter any issues:

- **Ensure all libraries are correctly installed and up to date**.
- **Verify wiring connections**, particularly for SPI and I2S peripherals.
- **Check the serial monitor for debugging messages** during setup and operation.
- **Consult the documentation** for the respective libraries and hardware modules.

For further assistance, feel free to open an issue in the repository or contact the project maintainers.