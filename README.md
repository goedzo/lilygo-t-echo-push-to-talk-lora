# LoRa PTT Communication Project with NRF52840 on T-Echo

## Overview

This project implements a Push-to-Talk (PTT) walkie-talkie system using LoRa communication on a LilyGO T-Echo board with an NRF52840 microcontroller. The project is designed to work with an e-paper display (e.g., GxEPD2 1.54" display) and supports different operating modes, including PTT, text message display (TXT), raw data display (RAW), and a test mode (TST).

## Features

- **LoRa Communication**: Enables long-range, low-power communication between devices using the SX1262 LoRa module.
- **PTT Mode**: Allows voice communication using a push-to-talk mechanism, with audio encoding/decoding using Codec2.
- **TXT Mode**: Displays text messages received over LoRa.
- **RAW Mode**: Displays raw data packets received over LoRa without filtering.
- **Test Mode**: Sends periodic test messages to verify communication.
- **E-Paper Display**: Displays current mode, messages, and other relevant information on an e-paper display.

## Hardware Requirements

- **Microcontroller**: LilyGO T-Echo with NRF52840
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
- **AceButton Library**: For handling button presses
- **PCF8563 Library**: For RTC if used in your project
- **SerialFlash Library**: For interacting with external flash memory
- **TinyGPSPlus Library**: For GPS modules, if applicable
- **SoftSPI Library**: For software-based SPI communication
- **Button2 Library**: Optional, depending on your button handling needs
- **SPI.h** and **Wire.h**: Standard Arduino libraries for SPI and I2C communication

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
   - Copy all the folders from the `lib` directory in this project to your Arduino libraries directory:
     ```
     C:\Users\<YourName>\Documents\Arduino\libraries
     ```
   - The required libraries include:
     - arduino-lmic
     - AceButton
     - Adafruit_BME280_Library
     - Adafruit_BusIO
     - Adafruit_EPD
     - Adafruit-GFX-Library
     - Button2
     - GxEPD
     - PCF8563_Library
     - RadioLib
     - SerialFlash
     - SoftSPI
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

- **Library Files**: Ensure that the files in the `lib` directory are correctly placed in your Arduino libraries directory, as they include crucial dependencies.
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
Handles audio capture and playback using the NRF52840's I2S interface, with Codec2 encoding/decoding.

### `settings.cpp`
Manages user settings, including mode switching and configuration adjustments.

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
- **Adafruit nRF52 Libraries**: The [Adafruit GitHub repository](https://github.com/adafruit/Adafruit_nRF52_Arduino) contains the official libraries