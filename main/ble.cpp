#include "ble.h"
#include <bluefruit.h>

// Create BLE service and characteristic
BLEService bleService("1235");
BLECharacteristic bleCharacteristic("ABCE");

void onCharacteristicWritten(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len);
void sendNotificationToApp(const char* message);  // Function to send message to app

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
    bleCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | CHR_PROPS_NOTIFY);  // Enable notifications
    bleCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    bleCharacteristic.setMaxLen(100);  // Set max length of messages
    bleCharacteristic.setFixedLen(100);
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

    // Check if the data is fully printable
    if (isDataPrintable(data, len)) {
        // Convert byte array to String assuming it is printable
        String receivedValue;
        for (int i = 0; i < len; i++) {
            receivedValue += (char)data[i];
        }
        Serial.print("Received string: ");
        Serial.println(receivedValue);

        // Respond back to the app with a notification
        sendNotificationToApp("Message received!");

    } else {
        // Handle as binary data
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

// Function to send a notification to the app
void sendNotificationToApp(const char* message) {
    // Get the message length
    size_t messageLength = strlen(message);

    // Create a buffer to hold the message, the "~~" marker, and the null terminator
    uint8_t buffer[messageLength + 3];  // Extra 2 bytes for "~~" and 1 byte for the null terminator

    // Copy the message into the buffer
    memcpy(buffer, message, messageLength);

    // Append the "~~" marker to indicate the end of the message
    buffer[messageLength] = '~';
    buffer[messageLength + 1] = '~';

    // Null-terminate the string
    buffer[messageLength + 2] = '\0';  // Null-terminator

    // Send the buffer, excluding the null terminator
    bleCharacteristic.notify(buffer, messageLength + 3);  // Send only the message + "~~"
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
