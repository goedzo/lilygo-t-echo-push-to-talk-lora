#include "ble.h"
#include <bluefruit.h>

// Create BLE service and characteristic
BLEService bleService("1234");
BLECharacteristic bleCharacteristic("ABCD");

void onCharacteristicWritten(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len);

// Renamed helper function to check if the data contains printable characters
bool isDataPrintable(const uint8_t* data, int length);

void setupBLE() {
    // Initialize BLE
    Bluefruit.begin();
    Bluefruit.setTxPower(4);  // Set the TX power to max (4dBm)

    // Set device name using a unique MAC address
    char deviceName[25];
    uint64_t mac = NRF_FICR->DEVICEID[0];  // Get unique ID like MAC address
    snprintf(deviceName, sizeof(deviceName), "LilygoT-Echo-%08X", (unsigned long)mac);
    Bluefruit.setName(deviceName);

    // Set up BLE service and characteristic
    bleService.begin();

    // Define the properties and permissions of the characteristic
    bleCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
    bleCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    bleCharacteristic.setFixedLen(20);  // Set fixed length if needed
    bleCharacteristic.setWriteCallback(onCharacteristicWritten);  // Set the write callback
    bleCharacteristic.begin();

    // Add service to the advertising packet
    Bluefruit.Advertising.addService(bleService);
    Bluefruit.Advertising.addName();  // Advertise device name

    // Start advertising
    Bluefruit.Advertising.start();

    Serial.print("BLE Initialized with device name: ");
    Serial.println(deviceName);
}

void onCharacteristicWritten(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    // This callback is invoked when the characteristic is written to
    Serial.print("Characteristic written, length: ");
    Serial.println(len);

    // Check if the data is string-compatible (contains printable characters)
    bool isString = (data[len - 1] == '\0');  // Check for null-terminated string

    if (isString && isDataPrintable(data, len - 1)) {
        // Convert byte array to String
        String receivedValue = (char*)data;
        Serial.print("Received string: ");
        Serial.println(receivedValue);
    } else {
        // Handle binary data
        Serial.print("Received binary data: ");
        for (int i = 0; i < len; i++) {
            Serial.print("0x");
            if (data[i] < 16) Serial.print("0");
            Serial.print(data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}

// Helper function to check if the data contains printable characters
bool isDataPrintable(const uint8_t* data, int length) {
    for (int i = 0; i < length; i++) {
        if (!isPrintable(data[i])) {  // Use built-in Arduino isPrintable for individual bytes
            return false;  // Non-printable character found
        }
    }
    return true;
}

