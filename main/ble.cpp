#include "ble.h"
#include <Arduino.h>
#include <BLEPeripheral.h>

// Define BLE service and characteristic UUIDs
BLEPeripheral blePeripheral;
BLEService bleService("1234");
BLECharacteristic bleCharacteristic("ABCD", BLEWrite | BLERead, 20);

void setupBLE() {
    char deviceName[25];
    
    // Get the unique MAC address of the device and use it as part of the BLE name
    uint64_t mac = NRF_FICR->DEVICEID[0];  // Get part of the unique ID (like MAC address)
    snprintf(deviceName, sizeof(deviceName), "LilygoT-Echo-%08X", (unsigned long)mac);

    // Set the unique name
    blePeripheral.setLocalName(deviceName);
    blePeripheral.setAdvertisedServiceUuid(bleService.uuid());
    blePeripheral.addAttribute(bleService);
    blePeripheral.addAttribute(bleCharacteristic);
    blePeripheral.begin();
    
    Serial.print("BLE Initialized with device name: ");
    Serial.println(deviceName);
}

void handleBLE() {
    BLECentral central = blePeripheral.central();
    if (central) {
        Serial.print("Connected to central: ");
        Serial.println(central.address());

        while (central.connected()) {
            if (bleCharacteristic.written()) {
                String receivedValue = bleCharacteristic.value();
                Serial.print("Received: ");
                Serial.println(receivedValue);
            }
        }

        Serial.print("Disconnected from central: ");
        Serial.println(central.address());
    }
}
