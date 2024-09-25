#ifndef UTILITY_H
#define UTILITY_H

#include <Arduino.h>
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "HIDTypes.h"  // Ensure you include this for keymap access

// Declare the necessary functions
String addressToString(esp_bd_addr_t addr);
bool isDeviceInTriedList(String addr);
void addDeviceToTriedList(String addr);
String getDeviceNameFromEIR(uint8_t *eir, uint8_t eir_len);
String getClassOfDeviceDescription(uint32_t cod);
void printFullDeviceInfo(const String& deviceName, const String& deviceAddr, const String& rawCod, const String& deviceClass, int8_t rssi);

// Declare charToHID
uint8_t charToHID(char key);

#endif  // UTILITY_H
