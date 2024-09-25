#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <BluetoothSerial.h>
#include <Arduino.h>
#include "display.h"
#include "utility.h"

void initBluetooth();
void scanClassicBT();
void classicBTDeviceFound(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
void checkAndConnectKeyboard(const String& btDeviceAddr, uint32_t cod, const String& btDeviceName);
extern BluetoothSerial SerialBT;

#endif  // BLUETOOTH_H
