#ifndef BLE_H
#define BLE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include <Arduino.h>
#include "utility.h"

void initBLE();
void sendKeyToBLE(char incomingChar);

#endif  // BLE_H
