#include "ble.h"
#include "utility.h"

BLEHIDDevice* hid;
BLEServer* pServer;

void initBLE() {
  BLEDevice::init("ESP32_BLE_Keyboard");
  pServer = BLEDevice::createServer();
  hid = new BLEHIDDevice(pServer);
  
  hid->manufacturer()->setValue("ESP32 Classic-to-BLE");
  hid->pnp(0x01, 0x02E5, 0xABCD, 0x0110);
  hid->hidInfo(0x00, 0x01);

  hid->startServices();
  BLEDevice::getAdvertising()->start();
  Serial.println("BLE HID started.");
}

void sendKeyToBLE(char incomingChar) {
  uint8_t key[] = {0, 0, charToHID(incomingChar), 0, 0, 0, 0, 0};
  hid->inputReport(1)->setValue(key, sizeof(key));
  hid->inputReport(1)->notify();
  
  delay(100);
  uint8_t keyRelease[] = {0, 0, 0, 0, 0, 0, 0, 0};
  hid->inputReport(1)->setValue(keyRelease, sizeof(keyRelease));
  hid->inputReport(1)->notify();
}
