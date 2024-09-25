#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <Arduino.h>
#include <BluetoothSerial.h>
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

// Initialize Classic Bluetooth and start scanning
void initBluetooth();

// Function to scan for Classic Bluetooth devices
void scanClassicBT();

// Callback when a Classic Bluetooth device is found
void classicBTDeviceFound(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

// Function to handle Bluetooth PIN code requests (for pairing)
void btPinCodeRequestCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

// Function to initialize Bluetooth pairing with the keyboard
void initBluetoothPairing();


void initiatePairing(const String& btDeviceAddr);

// Function to check and connect to a Bluetooth keyboard with pairing support
void checkAndConnectKeyboard(const String& btDeviceAddr, uint32_t cod, const String& btDeviceName, int8_t rssi);

// Declaration of the global Bluetooth Serial object
extern BluetoothSerial SerialBT;

#endif  // BLUETOOTH_H
