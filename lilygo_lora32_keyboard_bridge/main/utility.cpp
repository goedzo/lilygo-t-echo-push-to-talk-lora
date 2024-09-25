#include "utility.h"
#include "globals.h"  // Include the global variables

#include <HIDKeyboardTypes.h>  // From ESP32 BLE HID library

// Convert Bluetooth address to a String
String addressToString(esp_bd_addr_t addr) {
    char str[18];
    snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return String(str);
}

// Check if the device is already in the tried list
bool isDeviceInTriedList(String addr) {
    extern String triedDevices[];  // Assuming this array is defined elsewhere
    extern int triedDeviceCount;

    for (int i = 0; i < triedDeviceCount; i++) {
        if (triedDevices[i] == addr) {
            return true;  // Device already tried
        }
    }
    return false;
}

// Add a device to the tried list
void addDeviceToTriedList(String addr) {
    extern String triedDevices[];  // Assuming this array is defined elsewhere
    extern int triedDeviceCount;
    extern const int MAX_TRIED_DEVICES;

    if (triedDeviceCount < MAX_TRIED_DEVICES) {
        triedDevices[triedDeviceCount] = addr;
        triedDeviceCount++;
    }
}

// Extract the device name from EIR data
String getDeviceNameFromEIR(uint8_t *eir, uint8_t eir_len) {
    uint8_t length = 0;
    uint8_t* name_data = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &length);

    if (name_data && length > 0) {
        char deviceName[248];
        strncpy(deviceName, (char*)name_data, length);
        deviceName[length] = '\0';  // Null-terminate the string
        return String(deviceName);
    }

    return String("(unknown)");
}

// Decode and return a human-readable description of the class of device (CoD)
String getClassOfDeviceDescription(uint32_t cod) {
    String deviceClassStr;
    
    // Extract Major Service Class (bits 13 to 23)
    uint8_t majorServiceClass = (cod >> 13) & 0x7F;
    
    // Extract Major Device Class (bits 8 to 12)
    uint8_t majorDeviceClass = (cod >> 8) & 0x1F;
    
    // Extract Minor Device Class (bits 2 to 7)
    uint8_t minorDeviceClass = (cod >> 2) & 0x3F;

    // Append major service class description
    if (majorServiceClass & 0x20) deviceClassStr += "Audio, ";
    if (majorServiceClass & 0x40) deviceClassStr += "Telephony, ";
    if (majorServiceClass & 0x10) deviceClassStr += "Networking, ";
    if (majorServiceClass & 0x08) deviceClassStr += "Object Transfer, ";
    if (majorServiceClass & 0x04) deviceClassStr += "Capturing, ";
    if (majorServiceClass & 0x02) deviceClassStr += "Positioning, ";
    if (majorServiceClass & 0x01) deviceClassStr += "Limited Discovery, ";

    // Append major device class description
    deviceClassStr += "Major: ";
    switch (majorDeviceClass) {
        case 0x00: deviceClassStr += "Miscellaneous"; break;
        case 0x01: deviceClassStr += "Computer"; break;
        case 0x02: deviceClassStr += "Phone"; break;
        case 0x03: deviceClassStr += "LAN/Network"; break;
        case 0x04: deviceClassStr += "Audio/Video"; break;
        case 0x05:  // Peripheral device, check for keyboard or mouse
            deviceClassStr += "Peripheral (";
            switch (minorDeviceClass) {
                case 0x00: deviceClassStr += "Uncategorized"; break;
                case 0x01: deviceClassStr += "Keyboard"; break;
                case 0x02: deviceClassStr += "Mouse"; break;
                case 0x03: deviceClassStr += "Keyboard/Mouse Combo"; break;
                case 0x04: deviceClassStr += "Joystick"; break;
                case 0x05: deviceClassStr += "Gamepad"; break;
                case 0x06: deviceClassStr += "Remote Control"; break;
                case 0x07: deviceClassStr += "Sensing Device"; break;
                case 0x08: deviceClassStr += "Digitizer Tablet"; break;
                case 0x09: deviceClassStr += "Card Reader"; break;
                case 0x0A: deviceClassStr += "Digital Pen"; break;
                case 0x0B: deviceClassStr += "Handheld Scanner"; break;
                case 0x0C: deviceClassStr += "Handheld Gestural Input Device"; break;
                default: deviceClassStr += "Other Peripheral"; break;
            }
            deviceClassStr += ")";
            break;
        case 0x06: deviceClassStr += "Imaging"; break;
        case 0x07: deviceClassStr += "Wearable"; break;
        case 0x08: deviceClassStr += "Toy"; break;
        case 0x09: deviceClassStr += "Health"; break;
        default: deviceClassStr += "Unknown"; break;
    }

    return deviceClassStr;
}

// Print detailed information about a Bluetooth device
void printFullDeviceInfo(const String& deviceName, const String& deviceAddr, const String& rawCod, const String& deviceClass, int8_t rssi) {
    Serial.printf("Device Name: %s\n", deviceName.c_str());
    Serial.printf("Device Address: %s\n", deviceAddr.c_str());
    Serial.printf("Class of Device (Raw): %s\n", rawCod.c_str());
    Serial.printf("Class of Device: %s\n", deviceClass.c_str());
    Serial.printf("RSSI: %d dBm\n", rssi);
}

// Convert an ASCII character to an HID keycode
uint8_t charToHID(char key) {
    // Example HID conversion table (modify according to your HID keymap)
    if (key >= 'a' && key <= 'z') {
        return keymap[key - 'a' + 0x04].usage;  // For lowercase letters
    } else if (key >= 'A' && key <= 'Z') {
        return keymap[key - 'A' + 0x04].usage;  // For uppercase letters
    } else if (key >= '0' && key <= '9') {
        return keymap[key - '0' + 0x27].usage;  // For digits
    }

    // For special characters (space, enter, etc.)
    switch (key) {
        case ' ':
            return 0x2C;  // Space bar
        case '\n':
            return 0x28;  // Enter key
        default:
            return 0;  // For unhandled characters
    }
}
