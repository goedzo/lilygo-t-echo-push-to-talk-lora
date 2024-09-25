#include "bluetooth.h"
#include "utility.h"
#include "display.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

BluetoothSerial SerialBT;  // Classic Bluetooth object for scanning and connections

void initBluetooth() {
    if (!SerialBT.begin("ESP32_BT_Classic")) {
        Serial.println("An error occurred initializing Classic Bluetooth");
        return;
    }
    esp_bt_gap_register_callback(classicBTDeviceFound);
    Serial.println("Classic Bluetooth started. Scanning for keyboards...");
    displayInfo("", "", "", "");  // Initial display message
}

void scanClassicBT() {
    if (!SerialBT.hasClient()) {
        Serial.println("Scanning for Classic Bluetooth devices...");
        displayInfo("", "", "", "");  // Show scanning message
        esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);  // Start scanning
    }
}

// Callback when a Classic Bluetooth device is found
void classicBTDeviceFound(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    if (event == ESP_BT_GAP_DISC_RES_EVT) {
        String btDeviceAddr = addressToString(param->disc_res.bda);
        if (isDeviceInTriedList(btDeviceAddr)) return;
        addDeviceToTriedList(btDeviceAddr);

        String btDeviceName = "(unknown)";
        String deviceClassStr = "(unknown)";
        String rawCodStr = "(unknown)";
        int8_t rssi = -128;
        uint32_t cod = 0;

        for (int i = 0; i < param->disc_res.num_prop; i++) {
            esp_bt_gap_dev_prop_t *p = &param->disc_res.prop[i];
            switch (p->type) {
                case ESP_BT_GAP_DEV_PROP_EIR:
                    btDeviceName = getDeviceNameFromEIR(static_cast<uint8_t*>(p->val), p->len);
                    break;
                case ESP_BT_GAP_DEV_PROP_COD:
                    cod = *(uint32_t *)(p->val);
                    rawCodStr = String(cod, HEX);
                    deviceClassStr = getClassOfDeviceDescription(cod);
                    break;
                case ESP_BT_GAP_DEV_PROP_RSSI:
                    rssi = *(int8_t *)(p->val);
                    break;
            }
        }
        
        // Retry retrieving device name if still unknown
        if (btDeviceName == "(unknown)") {
            btDeviceName = getDeviceNameFromEIR(static_cast<uint8_t*>(param->disc_res.prop[0].val), param->disc_res.prop[0].len);
        }

        printFullDeviceInfo(btDeviceName, btDeviceAddr, rawCodStr, deviceClassStr, rssi);
        displayInfo(btDeviceName, btDeviceAddr, rawCodStr, deviceClassStr);
        checkAndConnectKeyboard(btDeviceAddr, cod, btDeviceName, rssi);
    }
}

// Function to handle Bluetooth PIN code requests (optional for keyboards that require a PIN)
void btPinCodeRequestCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    if (event == ESP_BT_GAP_PIN_REQ_EVT) {
        Serial.println("Bluetooth PIN Code Request received.");

        // Set a default PIN code (if necessary, you can also ask the user for input)
        uint8_t pinCode[4] = { '1', '2', '3', '4' };  // Example PIN: 1234
        esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pinCode);
        Serial.println("PIN code '1234' sent for pairing.");
    }
}

// Function to initiate pairing with the device
void initBluetoothPairing() {
    // Set up Secure Simple Pairing (SSP) with no input/output for devices like keyboards
    uint8_t ioCapability = ESP_BT_IO_CAP_NONE;  // No I/O capabilities for a keyboard
    esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &ioCapability, sizeof(uint8_t));

    // Register callback to handle pairing requests
    esp_bt_gap_register_callback(btPinCodeRequestCallback);

    // Enable visibility for the device
    esp_bt_dev_set_device_name("ESP32_Keyboard");
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
}

void removeBondedDevice(const String& btDeviceAddrStr) {
    esp_bd_addr_t btDeviceAddr;
    // Convert the string address "XX:XX:XX:XX:XX:XX" to byte array
    sscanf(btDeviceAddrStr.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
           (unsigned int*)&btDeviceAddr[0],
           (unsigned int*)&btDeviceAddr[1],
           (unsigned int*)&btDeviceAddr[2],
           (unsigned int*)&btDeviceAddr[3],
           (unsigned int*)&btDeviceAddr[4],
           (unsigned int*)&btDeviceAddr[5]);

    // Remove the bonded device
    esp_bt_gap_remove_bond_device(btDeviceAddr);
}


// Updated function to check and connect to the keyboard with pairing support
void checkAndConnectKeyboard(const String& btDeviceAddr, uint32_t cod, const String& btDeviceName, int8_t rssi) {
    uint8_t majorDeviceClass = (cod >> 8) & 0x1F;
    uint8_t minorDeviceClass = (cod >> 2) & 0x3F;

    if (majorDeviceClass == 0x05 && (minorDeviceClass == 0x01 || minorDeviceClass == 0x03 || minorDeviceClass == 0x10)) {
        Serial.printf("Attempting to pair and connect to Bluetooth Keyboard: %s\n", btDeviceName.c_str());

        if (!SerialBT.connected()) {
            // Log the Bluetooth address, class of device, and RSSI
            Serial.printf("Device Address: %s, Class of Device: 0x%X, RSSI: %d dBm\n", btDeviceAddr.c_str(), cod, rssi);

            // Initialize pairing process if needed
            initBluetoothPairing();

            // Remove any previously bonded devices to ensure a clean connection
            removeBondedDevice(btDeviceAddr);

            // Retry connection attempts (3 retries, 10 seconds timeout)
            int retries = 3;
            bool connected = false;
            while (retries-- > 0 && !connected) {
                Serial.printf("Initiating connection to %s (%d retries left)...\n", btDeviceAddr.c_str(), retries);
                SerialBT.connect(btDeviceAddr.c_str());

                // Increase delay to 10 seconds
                delay(10000);

                if (SerialBT.connected()) {
                    Serial.println("Connected successfully!");
                    connected = true;
                    break;
                } else {
                    Serial.println("Failed to connect.");

                    // Check if the failure is due to weak signal strength (RSSI)
                    if (rssi < -75) {
                        Serial.println("Potential reason: Signal strength too weak (RSSI < -75 dBm).");
                    } else {
                        Serial.println("Potential reason: Device may not be accepting connections or busy.");
                    }

                    // Additional diagnostics
                    if (!SerialBT.hasClient()) {
                        Serial.println("Potential reason: Device no longer visible or out of range.");
                    } else {
                        Serial.println("Potential reason: Pairing issue or unsupported Bluetooth protocol.");
                    }
                }
            }
            if (!connected) {
                Serial.println("Failed to connect after multiple attempts.");
            }
        } else {
            Serial.println("Already connected to a device.");
        }
    } else {
        Serial.println("Device is not a Bluetooth keyboard.");
    }
}

